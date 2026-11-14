// Jolt headers first: raylib defines a few macros (PI, ...) that we keep out of Jolt's way.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "engine/log.h"
#include "engine/physics.h"

namespace lk {
namespace {
namespace Layers {
constexpr JPH::ObjectLayer NON_MOVING = 0;
constexpr JPH::ObjectLayer MOVING = 1;
constexpr JPH::ObjectLayer COUNT = 2;
}  // namespace Layers

namespace BPLayers {
constexpr JPH::BroadPhaseLayer NON_MOVING(0);
constexpr JPH::BroadPhaseLayer MOVING(1);
constexpr unsigned COUNT = 2;
}  // namespace BPLayers

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    unsigned GetNumBroadPhaseLayers() const override { return BPLayers::COUNT; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override {
        return l == Layers::NON_MOVING ? BPLayers::NON_MOVING : BPLayers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l) const override {
        return l == BPLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
    }
#endif
};

class ObjVsBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override {
        if (a == Layers::NON_MOVING) return b == BPLayers::MOVING;
        return true;
    }
};

class ObjPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (a == Layers::NON_MOVING) return b == Layers::MOVING;
        return true;
    }
};

void TraceImpl(const char* fmt, ...) {
    va_list list;
    va_start(list, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, list);
    va_end(list);
    LK_DEBUG("jolt: %s", buf);
}

inline JPH::Vec3 J(Vector3 v) { return JPH::Vec3(v.x, v.y, v.z); }
inline Vector3 R(JPH::Vec3 v) { return Vector3{v.GetX(), v.GetY(), v.GetZ()}; }

JPH::Quat YawPitch(float yawDeg, float pitchDeg) {
    return JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::DegreesToRadians(yawDeg)) *
           JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::DegreesToRadians(pitchDeg));
}

int g_joltRefs = 0;
}  // namespace

struct PhysicsImpl {
    BPLayerInterface bpl;
    ObjVsBPFilter ovb;
    ObjPairFilter opf;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystemSingleThreaded> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    JPH::CharacterVsCharacterCollisionSimple charVsChar;
    struct PendingBox {
        JPH::Vec3 c, h;
        JPH::Quat q;
    };
    std::vector<PendingBox> pending;
    int staticBoxes = 0;
    JPH::BodyID staticBody;
    std::vector<JPH::BodyID> toggles;
};

struct CharacterImpl {
    JPH::Ref<JPH::CharacterVirtual> ch;
    bool enabled = true;
};

Physics::Physics() = default;
Physics::~Physics() { Shutdown(); }

bool Physics::Init() {
    if (g_joltRefs++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceImpl;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    impl_ = std::make_unique<PhysicsImpl>();
    impl_->temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    impl_->jobs = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    impl_->system = std::make_unique<JPH::PhysicsSystem>();
    impl_->system->Init(4096, 0, 4096, 4096, impl_->bpl, impl_->ovb, impl_->opf);
    impl_->system->SetGravity(JPH::Vec3(0, -9.81f, 0));
    return true;
}

void Physics::Shutdown() {
    if (!impl_) return;
    ClearToggles();
    ClearStatic();
    impl_.reset();
    if (--g_joltRefs == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void Physics::ClearStatic() {
    auto& bi = impl_->system->GetBodyInterface();
    if (!impl_->staticBody.IsInvalid()) {
        bi.RemoveBody(impl_->staticBody);
        bi.DestroyBody(impl_->staticBody);
        impl_->staticBody = JPH::BodyID();
    }
    impl_->pending.clear();
    impl_->staticBoxes = 0;
}

void Physics::AddStaticBox(Vector3 c, Vector3 h, float yawDeg, float pitchDeg) {
    JPH::Vec3 he(std::max(h.x, 0.06f), std::max(h.y, 0.06f), std::max(h.z, 0.06f));
    impl_->pending.push_back({J(c), he, YawPitch(yawDeg, pitchDeg)});
}

int Physics::StaticBoxCount() const { return impl_->staticBoxes; }

void Physics::BuildStatic() {
    if (impl_->pending.empty()) return;
    JPH::StaticCompoundShapeSettings settings;
    for (auto& b : impl_->pending) settings.AddShape(b.c, b.q, new JPH::BoxShape(b.h, 0.02f));
    auto res = settings.Create();
    if (res.HasError()) {
        LK_ERROR("physics: static compound failed: %s", res.GetError().c_str());
        return;
    }
    JPH::BodyCreationSettings bcs(res.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                  Layers::NON_MOVING);
    auto& bi = impl_->system->GetBodyInterface();
    impl_->staticBody = bi.CreateAndAddBody(bcs, JPH::EActivation::DontActivate);
    impl_->staticBoxes = int(impl_->pending.size());
    impl_->pending.clear();
    impl_->system->OptimizeBroadPhase();
}

int Physics::AddToggleBox(Vector3 c, Vector3 h, float yawDeg, bool enabled) {
    JPH::BodyCreationSettings bcs(new JPH::BoxShape(JPH::Vec3(h.x, h.y, h.z), 0.02f), JPH::RVec3(c.x, c.y, c.z),
                                  YawPitch(yawDeg, 0.0f), JPH::EMotionType::Static, Layers::NON_MOVING);
    auto& bi = impl_->system->GetBodyInterface();
    JPH::Body* body = bi.CreateBody(bcs);
    if (!body) return -1;
    if (enabled) bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    impl_->toggles.push_back(body->GetID());
    return int(impl_->toggles.size()) - 1;
}

void Physics::SetToggle(int handle, bool enabled) {
    if (handle < 0 || handle >= int(impl_->toggles.size())) return;
    auto& bi = impl_->system->GetBodyInterface();
    JPH::BodyID id = impl_->toggles[size_t(handle)];
    bool added = bi.IsAdded(id);
    if (enabled && !added) bi.AddBody(id, JPH::EActivation::DontActivate);
    if (!enabled && added) bi.RemoveBody(id);
}

void Physics::ClearToggles() {
    auto& bi = impl_->system->GetBodyInterface();
    for (auto id : impl_->toggles) {
        if (bi.IsAdded(id)) bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    impl_->toggles.clear();
}

RayHit Physics::RayCast(Vector3 from, Vector3 to) const {
    RayHit out;
    JPH::RRayCast ray{J(from), J(to) - J(from)};
    JPH::RayCastResult res;
    if (impl_->system->GetNarrowPhaseQuery().CastRay(ray, res)) {
        out.hit = true;
        out.fraction = res.mFraction;
        JPH::Vec3 p = ray.GetPointOnRay(res.mFraction);
        out.point = R(p);
        JPH::BodyLockRead lock(impl_->system->GetBodyLockInterface(), res.mBodyID);
        if (lock.Succeeded()) out.normal = R(lock.GetBody().GetWorldSpaceSurfaceNormal(res.mSubShapeID2, p));
    }
    return out;
}

RayHit Physics::SphereCast(Vector3 from, Vector3 to, float radius) const {
    RayHit out;
    JPH::RefConst<JPH::Shape> sphere = new JPH::SphereShape(radius);
    JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(sphere, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(J(from)),
                                                                J(to) - J(from));
    JPH::ShapeCastSettings settings;
    settings.mBackFaceModeTriangles = JPH::EBackFaceMode::IgnoreBackFaces;
    settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    impl_->system->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector);
    if (collector.HadHit()) {
        out.hit = true;
        out.fraction = collector.mHit.mFraction;
        out.point = R(collector.mHit.mContactPointOn2);
        JPH::Vec3 axis = collector.mHit.mPenetrationAxis;
        if (axis.LengthSq() > 1e-8f) out.normal = R(-axis.Normalized());
    }
    return out;
}

std::unique_ptr<Character> Physics::CreateCharacter(const CharacterDesc& d, Vector3 feet) {
    float halfH = std::max(0.05f, d.height * 0.5f - d.radius);
    JPH::RotatedTranslatedShapeSettings rts(JPH::Vec3(0, halfH + d.radius, 0), JPH::Quat::sIdentity(),
                                            new JPH::CapsuleShape(halfH, d.radius));
    auto shape = rts.Create();
    if (shape.HasError()) {
        LK_ERROR("physics: character shape: %s", shape.GetError().c_str());
        return nullptr;
    }
    JPH::CharacterVirtualSettings s;
    s.mShape = shape.Get();
    s.mMaxSlopeAngle = JPH::DegreesToRadians(d.maxSlopeDeg);
    s.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -d.radius);
    s.mMass = d.mass;
    s.mMaxStrength = 200.0f;
    s.mCharacterPadding = 0.02f;
    s.mPenetrationRecoverySpeed = 1.0f;
    s.mPredictiveContactDistance = 0.1f;
    s.mEnhancedInternalEdgeRemoval = true;
    auto c = std::make_unique<Character>();
    c->impl_ = std::make_unique<CharacterImpl>();
    c->phys_ = this;
    c->impl_->ch = new JPH::CharacterVirtual(&s, J(feet), JPH::Quat::sIdentity(), 0, impl_->system.get());
    c->impl_->ch->SetCharacterVsCharacterCollision(&impl_->charVsChar);
    impl_->charVsChar.Add(c->impl_->ch);
    return c;
}

void Physics::Step(float dt) {
    impl_->system->Update(dt, 1, impl_->temp.get(), impl_->jobs.get());
}

Character::~Character() {
    if (impl_ && phys_ && phys_->impl_ && impl_->enabled) phys_->impl_->charVsChar.Remove(impl_->ch);
}

Vector3 Character::Position() const { return R(impl_->ch->GetPosition()); }

void Character::SetPosition(Vector3 feet) {
    impl_->ch->SetPosition(J(feet));
    impl_->ch->SetLinearVelocity(JPH::Vec3::sZero());
}

Vector3 Character::Velocity() const { return R(impl_->ch->GetLinearVelocity()); }

bool Character::OnGround() const { return impl_->ch->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround; }

void Character::SetEnabled(bool e) {
    if (e == impl_->enabled) return;
    impl_->enabled = e;
    if (e) phys_->impl_->charVsChar.Add(impl_->ch);
    else phys_->impl_->charVsChar.Remove(impl_->ch);
}

bool Character::Enabled() const { return impl_->enabled; }

void Character::Move(Vector3 desired, float dt, float gravity) {
    auto* p = phys_->impl_.get();
    auto& ch = *impl_->ch;
    ch.UpdateGroundVelocity();
    JPH::Vec3 cur = ch.GetLinearVelocity();
    float vy = cur.GetY();
    // Grounded: ride the ground (no gravity, so idle characters do not creep down slopes).
    if (ch.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) vy = ch.GetGroundVelocity().GetY();
    else vy += gravity * dt;
    vy = std::max(vy, -30.0f);
    ch.SetLinearVelocity(JPH::Vec3(desired.x, vy, desired.z));
    JPH::CharacterVirtual::ExtendedUpdateSettings ups;
    ups.mStickToFloorStepDown = JPH::Vec3(0, -0.35f, 0);
    ups.mWalkStairsStepUp = JPH::Vec3(0, 0.3f, 0);
    ch.ExtendedUpdate(dt, JPH::Vec3(0, gravity, 0), ups, p->system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                      p->system->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *p->temp);
}

}  // namespace lk
