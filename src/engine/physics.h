#pragma once
// Jolt Physics wrapper: static level colliders (built from tile data), toggleable static bodies
// (gates), ray and sphere casts, and JPH::CharacterVirtual for the knight and enemies.
#include <memory>
#include <vector>

#include "raylib.h"

namespace lk {

struct PhysicsImpl;
struct CharacterImpl;

struct RayHit {
    bool hit = false;
    float fraction = 1.0f;   // along the segment
    Vector3 point{};
    Vector3 normal{};
};

struct CharacterDesc {
    float radius = 0.3f;
    float height = 1.2f;       // total capsule height
    float maxSlopeDeg = 50.0f;
    float mass = 70.0f;
};

class Physics;

class Character {
public:
    ~Character();
    Vector3 Position() const;       // feet position
    void SetPosition(Vector3 feet);
    Vector3 Velocity() const;
    bool OnGround() const;
    // Move with a desired horizontal velocity; gravity and floor sticking handled here.
    void Move(Vector3 desiredHorizontal, float dt, float gravity);
    void SetEnabled(bool e);        // disabled characters are skipped by others
    bool Enabled() const;

private:
    friend class Physics;
    std::unique_ptr<CharacterImpl> impl_;
    Physics* phys_ = nullptr;
};

class Physics {
public:
    Physics();
    ~Physics();
    bool Init();
    void Shutdown();

    // Level geometry. Boxes accumulate until BuildStatic() bakes them into one body.
    void ClearStatic();
    void AddStaticBox(Vector3 center, Vector3 halfExtents, float yawDeg = 0.0f, float pitchDeg = 0.0f);
    void BuildStatic();
    int StaticBoxCount() const;

    // Toggleable static bodies (gates, fog wall). Returns a handle.
    int AddToggleBox(Vector3 center, Vector3 halfExtents, float yawDeg, bool enabled);
    void SetToggle(int handle, bool enabled);
    void ClearToggles();

    RayHit RayCast(Vector3 from, Vector3 to) const;
    RayHit SphereCast(Vector3 from, Vector3 to, float radius) const;

    std::unique_ptr<Character> CreateCharacter(const CharacterDesc& d, Vector3 feet);
    void Step(float dt);   // keeps the broadphase up to date

    PhysicsImpl* Impl() { return impl_.get(); }

private:
    friend class Character;
    std::unique_ptr<PhysicsImpl> impl_;
};

}  // namespace lk
