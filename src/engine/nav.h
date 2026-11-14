#pragma once
// Navigation: A* on the zone's tile grid across height layers (stairs link layers), with
// a grid line-of-sight shortcut so agents walk straight when they can.
#include <unordered_map>
#include <vector>

#include "core/zone_data.h"
#include "raylib.h"

namespace lk {

class Nav {
public:
    void Build(const ZoneData& zone);
    // Next point to steer toward on the way from `from` to `to` (returns `to` if direct).
    Vector3 NextWaypoint(Vector3 from, Vector3 to) const;
    bool Walkable(Vector3 p) const;
    // Full path as cell centres (for debugging / tests). Empty if unreachable.
    std::vector<Vector3> Path(Vector3 from, Vector3 to) const;
    void SetBlocked(int level, int cx, int cz, bool blocked);   // closed gates
    int NodeCount() const { return int(nodes_.size()); }
    Vector3 NodeCenter(int i) const { return nodes_[size_t(i)].center; }

private:
    struct Node {
        int level, cx, cz;
        Vector3 center;
        std::vector<int> links;
        bool blocked = false;
        bool stairs = false;
    };
    int Find(int level, int cx, int cz) const;
    int Locate(Vector3 p) const;
    bool LineClear(int a, int b) const;
    std::vector<int> AStar(int s, int g) const;
    static long long Key(int l, int x, int z) {
        return (static_cast<long long>(l) << 40) ^ (static_cast<long long>(x + 100000) << 20) ^ static_cast<long long>(z + 100000);
    }

    std::vector<Node> nodes_;
    std::unordered_map<long long, int> index_;
    float cell_ = 2.0f, layerH_ = 2.0f;
};

}  // namespace lk
