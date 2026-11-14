#include "game/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>

#include "engine/assets.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "rlgl.h"

namespace lk {
namespace {
constexpr int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N, E, S, W
// Yaw for a wall mesh (long axis = X) lying on the edge toward each dir.
constexpr float kWallYaw[4] = {0.0f, 90.0f, 180.0f, 270.0f};

unsigned Hash(int a, int b, int c) {
    unsigned h = 2166136261u;
    for (int v : {a, b, c}) {
        h ^= unsigned(v);
        h *= 16777619u;
    }
    return h;
}

Vector3 RotY(Vector3 v, float deg) {
    float r = deg * DEG2RAD, c = std::cos(r), s = std::sin(r);
    // raymath MatrixRotateY convention: x' = x c + z s, z' = -x s + z c
    return Vector3{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}
}  // namespace

Vector3 StairsRiseDir(int rotDeg) { return RotY(Vector3{0, 0, -1}, float(rotDeg)); }

World::~World() { Unload(); }

void World::Unload() {
    for (auto& c : chunks_)
        for (auto& m : c.meshes) UnloadMesh(m);
    chunks_.clear();
    props_.clear();
    pieceCount_ = 0;
}

std::string World::PiecePath(const std::string& piece) {
    std::string a = "kaykit_dungeon/" + piece + ".gltf.glb";
    if (std::filesystem::exists(paths::Asset(a))) return a;
    return "kaykit_dungeon/" + piece + ".glb";
}

Matrix World::PieceMatrix(Vector3 pos, float yawDeg, float scale) const {
    float s = zone_.modelScale * scale;
    return MatrixMultiply(MatrixMultiply(MatrixScale(s, s, s), MatrixRotateY(yawDeg * DEG2RAD)),
                          MatrixTranslate(pos.x, pos.y, pos.z));
}

void World::AddPiece(const std::string& piece, Vector3 pos, float yawDeg, std::vector<PieceInstance>& out) {
    ModelAsset* a = assets_->LoadModel(PiecePath(piece));
    if (!a) return;
    if (!hasAtlas_ && a->model.materialCount > 0) {
        atlas_ = a->model.materials[a->model.materialCount > 1 ? 1 : 0];
        hasAtlas_ = true;
    }
    out.push_back({a, PieceMatrix(pos, yawDeg)});
}

bool World::IsOpen(int level, int cx, int cz) const {
    TileKind k = zone_.KindAt(level, cx, cz);
    return k == TileKind::Floor || k == TileKind::Stairs;
}

bool World::StairsRisingInto(int level, int cx, int cz, int dx, int dz) const {
    // Is there a stairs tile at (cx,cz) on `level` that rises in direction (dx,dz)?
    const ZoneLayer* l = zone_.Layer(level);
    if (!l) return false;
    const LegendEntry* e = zone_.Legend(l->At(cx, cz));
    if (!e || e->kind != TileKind::Stairs) return false;
    Vector3 d = StairsRiseDir(e->rot);
    return int(std::lround(d.x)) == dx && int(std::lround(d.z)) == dz;
}

void World::BuildWalls(std::vector<PieceInstance>& pieces, Physics& physics) {
    const float cs = zone_.cellSize, lh = zone_.layerHeight;
    const float wallThick = 0.5f;  // 1 model unit * 0.5
    for (const auto& layer : zone_.layers) {
        int L = layer.level;
        int w = layer.Width(), d = layer.Depth();
        for (int lz = 0; lz < d; ++lz)
            for (int lx = 0; lx < w; ++lx) {
                int cx = layer.originX + lx, cz = layer.originZ + lz;
                const LegendEntry* e = zone_.Legend(layer.At(cx, cz));
                if (!e) continue;
                Vec3f c = zone_.CellCenter(L, cx, cz);
                Vector3 center{c.x, c.y, c.z};
                if (e->kind == TileKind::Floor) {
                    AddPiece(e->piece, center, float(e->rot), pieces);
                    physics.AddStaticBox(Vector3{c.x, c.y - 0.15f, c.z}, Vector3{cs * 0.5f, 0.15f, cs * 0.5f});
                } else if (e->kind == TileKind::Stairs) {
                    Vector3 rise = StairsRiseDir(e->rot);
                    Vector3 origin = Vector3Add(center, Vector3Scale(rise, cs * 0.5f));
                    AddPiece(e->piece, origin, float(e->rot), pieces);
                    // Ramp collider: 45 degree slab from the low edge to the high edge.
                    float run = cs, riseH = lh;
                    float len = std::sqrt(run * run + riseH * riseH);
                    float pitch = std::atan2(riseH, run) * RAD2DEG;
                    float halfT = 0.1f;
                    Vector3 n = Vector3Normalize(Vector3{-rise.x * riseH, run, -rise.z * riseH});
                    Vector3 mid = Vector3Add(center, Vector3{0, riseH * 0.5f, 0});
                    mid = Vector3Subtract(mid, Vector3Scale(n, halfT));
                    // Extend the slab past the low end so its end face sits under the floor; otherwise the
                    // character meets that downward-facing face at the seam and treats it as a wall.
                    const float ext = 0.4f;
                    Vector3 down = Vector3Normalize(Vector3{-rise.x * run, -riseH, -rise.z * run});
                    mid = Vector3Add(mid, Vector3Scale(down, ext * 0.5f));
                    physics.AddStaticBox(mid, Vector3{cs * 0.45f, halfT, len * 0.5f + ext * 0.5f}, float(e->rot), pitch);
                    // Floor under the low end so nothing falls through at the seam.
                    physics.AddStaticBox(Vector3{c.x, c.y - 0.15f, c.z}, Vector3{cs * 0.5f, 0.15f, cs * 0.5f});
                }
                if (e->kind != TileKind::Floor && e->kind != TileKind::Stairs) continue;

                for (int dir = 0; dir < 4; ++dir) {
                    int nx = cx + kDirs[dir][0], nz = cz + kDirs[dir][1];
                    if (IsOpen(L, nx, nz)) continue;
                    // Stairs: the high end opens onto the layer above; the low end must stay open.
                    if (e->kind == TileKind::Stairs) {
                        Vector3 r = StairsRiseDir(e->rot);
                        if (int(std::lround(r.x)) == kDirs[dir][0] && int(std::lround(r.z)) == kDirs[dir][1] &&
                            IsOpen(L + 1, nx, nz))
                            continue;
                    }
                    // Upper layer edge above an open lower cell -> railing + cliff face below.
                    bool lowerOpen = L > 0 && zone_.KindAt(L, nx, nz) == TileKind::Void && IsOpen(L - 1, nx, nz);
                    if (L > 0 && zone_.KindAt(L, nx, nz) == TileKind::Void &&
                        StairsRisingInto(L - 1, nx, nz, -kDirs[dir][0], -kDirs[dir][1]))
                        continue;  // top of a staircase arriving here
                    Vector3 edge = Vector3Add(center, Vector3{kDirs[dir][0] * cs * 0.5f, 0, kDirs[dir][1] * cs * 0.5f});
                    float yaw = kWallYaw[dir];
                    if (lowerOpen) {
                        AddPiece("barrier", Vector3Add(edge, Vector3{-kDirs[dir][0] * 0.15f, 0, -kDirs[dir][1] * 0.15f}), yaw,
                                 pieces);
                        Vector3 cliff = Vector3Add(edge, Vector3{0, -lh, 0});
                        AddPiece("wall", cliff, yaw + 180.0f, pieces);
                        Vector3 he = (dir % 2 == 0) ? Vector3{cs * 0.5f, 0.6f, 0.15f} : Vector3{0.15f, 0.6f, cs * 0.5f};
                        physics.AddStaticBox(Vector3Add(edge, Vector3{0, 0.6f, 0}), he);
                        Vector3 hc = (dir % 2 == 0) ? Vector3{cs * 0.5f, lh * 0.5f, wallThick * 0.5f}
                                                    : Vector3{wallThick * 0.5f, lh * 0.5f, cs * 0.5f};
                        physics.AddStaticBox(Vector3Add(cliff, Vector3{0, lh * 0.5f, 0}), hc);
                        continue;
                    }
                    std::string piece = "wall";
                    if (const LegendEntry* ne = zone_.Legend(zone_.Layer(L) ? zone_.Layer(L)->At(nx, nz) : ' ');
                        ne && !ne->variants.empty()) {
                        unsigned h = Hash(cx * 4 + dir, cz, L);
                        piece = ne->variants[h % ne->variants.size()];
                    }
                    AddPiece(piece, edge, yaw, pieces);
                    Vector3 he = (dir % 2 == 0) ? Vector3{cs * 0.5f + 0.25f, lh * 0.5f, wallThick * 0.5f + 0.05f}
                                                : Vector3{wallThick * 0.5f + 0.05f, lh * 0.5f, cs * 0.5f + 0.25f};
                    physics.AddStaticBox(Vector3Add(edge, Vector3{0, lh * 0.5f, 0}), he);
                }
            }
        // Corner posts where two wall edges meet at a convex/concave grid vertex.
        for (int lz = 0; lz <= d; ++lz)
            for (int lx = 0; lx <= w; ++lx) {
                int vx = layer.originX + lx, vz = layer.originZ + lz;  // vertex at cell (vx,vz) top-left corner
                bool open[4] = {IsOpen(L, vx - 1, vz - 1), IsOpen(L, vx, vz - 1), IsOpen(L, vx, vz), IsOpen(L, vx - 1, vz)};
                int n = open[0] + open[1] + open[2] + open[3];
                if (n == 0 || n == 4) continue;
                bool straight = (n == 2) && ((open[0] && open[1]) || (open[1] && open[2]) || (open[2] && open[3]) ||
                                             (open[3] && open[0]));
                if (straight) continue;
                Vector3 p{float(vx) * zone_.cellSize, float(L) * lh, float(vz) * zone_.cellSize};
                AddPiece("wall_corner_small", p, 0.0f, pieces);
            }
    }
}

void World::BuildChunks(const std::vector<PieceInstance>& pieces) {
    std::map<std::pair<int, int>, std::vector<const PieceInstance*>> bins;
    for (auto& p : pieces) {
        int kx = int(std::floor(p.m.m12 / chunkSize_)), kz = int(std::floor(p.m.m14 / chunkSize_));
        bins[{kx, kz}].push_back(&p);
    }
    for (auto& [key, list] : bins) {
        WorldChunk chunk;
        std::vector<float> pos, nrm, uv;
        std::vector<unsigned short> idx;
        BoundingBox bb{{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
        auto flush = [&]() {
            if (pos.empty()) return;
            Mesh m{};
            m.vertexCount = int(pos.size() / 3);
            m.triangleCount = int(idx.size() / 3);
            m.vertices = static_cast<float*>(MemAlloc(unsigned(pos.size() * sizeof(float))));
            m.normals = static_cast<float*>(MemAlloc(unsigned(nrm.size() * sizeof(float))));
            m.texcoords = static_cast<float*>(MemAlloc(unsigned(uv.size() * sizeof(float))));
            m.indices = static_cast<unsigned short*>(MemAlloc(unsigned(idx.size() * sizeof(unsigned short))));
            std::memcpy(m.vertices, pos.data(), pos.size() * sizeof(float));
            std::memcpy(m.normals, nrm.data(), nrm.size() * sizeof(float));
            std::memcpy(m.texcoords, uv.data(), uv.size() * sizeof(float));
            std::memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
            UploadMesh(&m, false);
            chunk.meshes.push_back(m);
            pos.clear();
            nrm.clear();
            uv.clear();
            idx.clear();
        };
        for (const PieceInstance* pi : list) {
            const Model& model = pi->asset->model;
            Matrix nm = MatrixTranspose(MatrixInvert(pi->m));
            for (int mi = 0; mi < model.meshCount; ++mi) {
                const Mesh& src = model.meshes[mi];
                if (pos.size() / 3 + size_t(src.vertexCount) > 60000) flush();
                unsigned short base = (unsigned short)(pos.size() / 3);
                for (int v = 0; v < src.vertexCount; ++v) {
                    Vector3 p{src.vertices[v * 3], src.vertices[v * 3 + 1], src.vertices[v * 3 + 2]};
                    p = Vector3Transform(p, pi->m);
                    pos.insert(pos.end(), {p.x, p.y, p.z});
                    bb.min = Vector3Min(bb.min, p);
                    bb.max = Vector3Max(bb.max, p);
                    Vector3 n{0, 1, 0};
                    if (src.normals) {
                        n = {src.normals[v * 3], src.normals[v * 3 + 1], src.normals[v * 3 + 2]};
                        Vector3 t = Vector3Transform(n, nm);
                        Vector3 o = Vector3Transform(Vector3Zero(), nm);
                        n = Vector3Normalize(Vector3Subtract(t, o));
                    }
                    nrm.insert(nrm.end(), {n.x, n.y, n.z});
                    if (src.texcoords) uv.insert(uv.end(), {src.texcoords[v * 2], src.texcoords[v * 2 + 1]});
                    else uv.insert(uv.end(), {0.0f, 0.0f});
                }
                if (src.indices) {
                    for (int t = 0; t < src.triangleCount * 3; ++t) idx.push_back((unsigned short)(base + src.indices[t]));
                } else {
                    for (int t = 0; t < src.vertexCount; ++t) idx.push_back((unsigned short)(base + t));
                }
            }
        }
        flush();
        chunk.bounds = bb;
        chunk.center = Vector3Scale(Vector3Add(bb.min, bb.max), 0.5f);
        chunks_.push_back(std::move(chunk));
    }
}

bool World::Build(const ZoneData& zone, AssetCache& assets, Physics& physics) {
    Unload();
    zone_ = zone;
    assets_ = &assets;
    physics.ClearStatic();
    std::vector<PieceInstance> pieces;
    BuildWalls(pieces, physics);
    pieceCount_ = int(pieces.size());
    BuildChunks(pieces);

    for (const auto& p : zone_.props) {
        WorldProp wp;
        wp.asset = assets.LoadModel(p.model);
        if (!wp.asset) continue;
        wp.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        wp.world = PieceMatrix(wp.pos, p.rot, p.scale);
        wp.id = p.id;
        BoundingBox b = wp.asset->bounds;
        float s = zone_.modelScale * p.scale;
        wp.radius = std::max(0.3f, Vector3Length(Vector3Subtract(b.max, b.min)) * 0.5f * s);
        if (p.extra.value("collide", false)) {
            Vector3 he = Vector3Scale(Vector3Subtract(b.max, b.min), 0.5f * s);
            Vector3 c = Vector3Scale(Vector3Add(b.max, b.min), 0.5f * s);
            c = RotY(c, p.rot);
            physics.AddStaticBox(Vector3Add(wp.pos, c), he, p.rot);
        }
        props_.push_back(wp);
    }
    physics.BuildStatic();
    LK_INFO("world: %s: %d pieces in %zu chunks, %zu props, %d colliders", zone_.name.c_str(), pieceCount_, chunks_.size(),
            props_.size(), physics.StaticBoxCount());
    return true;
}

void World::Draw(Renderer& r, Vector3 camPos, float drawDistance) const {
    DrawParams p;
    for (const auto& c : chunks_) {
        Vector3 closest = Vector3Clamp(camPos, c.bounds.min, c.bounds.max);
        if (Vector3Distance(closest, camPos) > drawDistance) continue;
        for (const auto& m : c.meshes) r.DrawStaticMesh(m, atlas_, MatrixIdentity(), c.center, p);
    }
    for (const auto& pr : props_) {
        if (Vector3Distance(pr.pos, camPos) > drawDistance + pr.radius) continue;
        r.DrawModel(*pr.asset, pr.world, nullptr, p);
    }
}

float World::FloorHeightAt(float x, float y, float z) const {
    int cx, cz;
    zone_.WorldToCell(x, z, cx, cz);
    float best = -1e9f;
    for (const auto& l : zone_.layers) {
        float h = float(l.level) * zone_.layerHeight;
        if (h > y + 0.5f) continue;
        TileKind k = zone_.KindAt(l.level, cx, cz);
        if (k == TileKind::Floor && h > best) best = h;
        if (k == TileKind::Stairs) {
            const LegendEntry* e = zone_.Legend(l.At(cx, cz));
            Vector3 rise = StairsRiseDir(e ? e->rot : 0);
            Vec3f c = zone_.CellCenter(l.level, cx, cz);
            float t = ((x - c.x) * rise.x + (z - c.z) * rise.z) / zone_.cellSize + 0.5f;
            float sh = h + std::clamp(t, 0.0f, 1.0f) * zone_.layerHeight;
            if (sh <= y + 0.6f && sh > best) best = sh;
        }
    }
    return best;
}

}  // namespace lk
