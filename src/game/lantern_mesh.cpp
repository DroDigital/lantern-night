#include "game/lantern_mesh.h"

#include "rlgl.h"

namespace lk {

void LanternMesh::Init() {
    glass_ = GenMeshCylinder(0.065f, 0.15f, 8);
    cap_ = GenMeshCone(0.09f, 0.07f, 8);
    base_ = GenMeshCylinder(0.085f, 0.03f, 8);
    ring_ = GenMeshTorus(0.012f, 0.06f, 6, 10);
    mat_ = LoadMaterialDefault();
    ready_ = true;
}

void LanternMesh::Unload() {
    if (!ready_) return;
    UnloadMesh(glass_);
    UnloadMesh(cap_);
    UnloadMesh(base_);
    UnloadMesh(ring_);
    mat_.shader = Shader{rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
    UnloadMaterial(mat_);
    ready_ = false;
}

void LanternMesh::Draw(Renderer& r, Vector3 p, float yaw, float flame, float flicker, Vector3 glow) const {
    if (!ready_) return;
    Matrix rot = MatrixRotateY(yaw);
    auto at = [&](float y) { return MatrixMultiply(rot, MatrixTranslate(p.x, p.y + y, p.z)); };
    Material m = mat_;
    DrawParams dp;
    // Glass: emissive with the flame.
    m.maps[MATERIAL_MAP_DIFFUSE].color = Color{255, 200, 140, 255};
    float g = (0.6f + 2.4f * flame) * flicker;
    dp.emissive = Vector3{glow.x * g, glow.y * g, glow.z * g};
    r.DrawStaticMesh(glass_, m, at(-0.075f), p, dp);
    dp.emissive = Vector3{0, 0, 0};
    m.maps[MATERIAL_MAP_DIFFUSE].color = Color{52, 46, 56, 255};
    r.DrawStaticMesh(cap_, m, at(0.075f), p, dp);
    r.DrawStaticMesh(base_, m, at(-0.1f), p, dp);
    Matrix ringM = MatrixMultiply(MatrixMultiply(MatrixRotateX(PI / 2), rot), MatrixTranslate(p.x, p.y + 0.17f, p.z));
    r.DrawStaticMesh(ring_, m, ringM, p, dp);
}

}  // namespace lk
