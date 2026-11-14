#pragma once
// Turns ZoneData into renderable chunk meshes (tiles merged per chunk, one atlas material),
// static Jolt colliders and per-cell walkability for the nav grid.
#include <string>
#include <vector>

#include "core/zone_data.h"
#include "raylib.h"
#include "raymath.h"

namespace lk {

class AssetCache;
class Physics;
class Renderer;
struct ModelAsset;

struct WorldChunk {
    std::vector<Mesh> meshes;
    BoundingBox bounds{};
    Vector3 center{};
};

struct WorldProp {
    ModelAsset* asset = nullptr;
    Matrix world = MatrixIdentity();
    Vector3 pos{};
    float radius = 1.0f;
    std::string id;
};

class World {
public:
    ~World();
    bool Build(const ZoneData& zone, AssetCache& assets, Physics& physics);
    void Unload();
    void Draw(Renderer& r, Vector3 camPos, float drawDistance) const;

    const ZoneData& Zone() const { return zone_; }
    // Floor height under a world point on the highest walkable layer at or below y+0.5 (or -1e9).
    float FloorHeightAt(float x, float y, float z) const;
    // Model path for a dungeon piece name ("floor_tile_large" -> "kaykit_dungeon/floor_tile_large.gltf.glb").
    static std::string PiecePath(const std::string& piece);
    Matrix PieceMatrix(Vector3 pos, float yawDeg, float scale = 1.0f) const;

    int ChunkCount() const { return int(chunks_.size()); }
    int PieceCount() const { return pieceCount_; }
    std::vector<WorldProp>& Props() { return props_; }

private:
    struct PieceInstance {
        ModelAsset* asset;
        Matrix m;
    };
    void AddPiece(const std::string& piece, Vector3 pos, float yawDeg, std::vector<PieceInstance>& out);
    void BuildChunks(const std::vector<PieceInstance>& pieces);
    void BuildWalls(std::vector<PieceInstance>& pieces, Physics& physics);
    bool IsOpen(int level, int cx, int cz) const;   // floor or stairs
    bool StairsRisingInto(int level, int cx, int cz, int dx, int dz) const;

    ZoneData zone_;
    AssetCache* assets_ = nullptr;
    std::vector<WorldChunk> chunks_;
    std::vector<WorldProp> props_;
    Material atlas_{};
    bool hasAtlas_ = false;
    int pieceCount_ = 0;
    float chunkSize_ = 16.0f;
};

// Unit direction a stairs tile rises toward, for its legend rotation.
Vector3 StairsRiseDir(int rotDeg);

}  // namespace lk
