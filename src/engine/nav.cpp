#include "engine/nav.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include "raymath.h"

namespace lk {

static Vector3 RiseDir(int rot) {
    float r = float(rot) * DEG2RAD;
    return Vector3{-std::sin(r), 0, -std::cos(r)};  // local -Z rotated by yaw
}

void Nav::Build(const ZoneData& z) {
    nodes_.clear();
    index_.clear();
    cell_ = z.cellSize;
    layerH_ = z.layerHeight;
    for (const auto& l : z.layers)
        for (int lz = 0; lz < l.Depth(); ++lz)
            for (int lx = 0; lx < int(l.rows[size_t(lz)].size()); ++lx) {
                int cx = l.originX + lx, cz = l.originZ + lz;
                TileKind k = z.KindAt(l.level, cx, cz);
                if (k != TileKind::Floor && k != TileKind::Stairs) continue;
                Node n;
                n.level = l.level;
                n.cx = cx;
                n.cz = cz;
                Vec3f c = z.CellCenter(l.level, cx, cz);
                n.center = Vector3{c.x, c.y, c.z};
                n.stairs = k == TileKind::Stairs;
                if (n.stairs) n.center.y += z.layerHeight * 0.5f;
                index_[Key(l.level, cx, cz)] = int(nodes_.size());
                nodes_.push_back(n);
            }
    const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (size_t i = 0; i < nodes_.size(); ++i) {
        Node& n = nodes_[i];
        if (n.stairs) {
            const LegendEntry* e = z.Legend(z.Layer(n.level)->At(n.cx, n.cz));
            Vector3 r = RiseDir(e ? e->rot : 0);
            int rx = int(std::lround(r.x)), rz = int(std::lround(r.z));
            int lo = Find(n.level, n.cx - rx, n.cz - rz);
            int hi = Find(n.level + 1, n.cx + rx, n.cz + rz);
            for (int o : {lo, hi})
                if (o >= 0) {
                    n.links.push_back(o);
                    nodes_[size_t(o)].links.push_back(int(i));
                }
            continue;
        }
        for (auto& d : d4) {
            int o = Find(n.level, n.cx + d[0], n.cz + d[1]);
            if (o >= 0 && !nodes_[size_t(o)].stairs) n.links.push_back(o);
        }
        // Diagonals only when both orthogonal neighbours are open (no corner cutting).
        for (int dx : {-1, 1})
            for (int dz : {-1, 1}) {
                int a = Find(n.level, n.cx + dx, n.cz), b = Find(n.level, n.cx, n.cz + dz);
                int o = Find(n.level, n.cx + dx, n.cz + dz);
                if (o >= 0 && a >= 0 && b >= 0 && !nodes_[size_t(o)].stairs && !nodes_[size_t(a)].stairs &&
                    !nodes_[size_t(b)].stairs)
                    n.links.push_back(o);
            }
    }
}

int Nav::Find(int level, int cx, int cz) const {
    auto it = index_.find(Key(level, cx, cz));
    return it == index_.end() ? -1 : it->second;
}

int Nav::Locate(Vector3 p) const {
    int cx = int(std::floor(p.x / cell_)), cz = int(std::floor(p.z / cell_));
    int best = -1;
    float bestD = 1e9f;
    for (int l = -1; l < 8; ++l) {
        int i = Find(l, cx, cz);
        if (i < 0) continue;
        float base = float(l) * layerH_;
        float top = nodes_[size_t(i)].stairs ? base + layerH_ : base;
        float d = (p.y + 0.3f < base) ? 1e6f : std::fabs(p.y - std::clamp(p.y, base, top));
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

bool Nav::Walkable(Vector3 p) const {
    int i = Locate(p);
    return i >= 0 && !nodes_[size_t(i)].blocked;
}

void Nav::SetBlocked(int level, int cx, int cz, bool blocked) {
    int i = Find(level, cx, cz);
    if (i >= 0) nodes_[size_t(i)].blocked = blocked;
}

bool Nav::LineClear(int a, int b) const {
    const Node& A = nodes_[size_t(a)];
    const Node& B = nodes_[size_t(b)];
    if (A.level != B.level || A.stairs || B.stairs) return a == b;
    int x0 = A.cx, z0 = A.cz, x1 = B.cx, z1 = B.cz;
    int dx = std::abs(x1 - x0), dz = std::abs(z1 - z0);
    int sx = x0 < x1 ? 1 : -1, sz = z0 < z1 ? 1 : -1;
    int err = dx - dz;
    while (true) {
        int i = Find(A.level, x0, z0);
        if (i < 0 || nodes_[size_t(i)].blocked || nodes_[size_t(i)].stairs) return false;
        if (x0 == x1 && z0 == z1) return true;
        int e2 = 2 * err;
        if (e2 > -dz && e2 < dx) {
            // Diagonal step: both side cells must be open (agents are wide).
            if (Find(A.level, x0 + sx, z0) < 0 || Find(A.level, x0, z0 + sz) < 0) return false;
        }
        if (e2 > -dz) {
            err -= dz;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            z0 += sz;
        }
    }
}

std::vector<int> Nav::AStar(int s, int g) const {
    std::vector<int> out;
    if (s < 0 || g < 0) return out;
    std::vector<float> gScore(nodes_.size(), 1e30f);
    std::vector<int> came(nodes_.size(), -1);
    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> open;
    auto h = [&](int i) { return Vector3Distance(nodes_[size_t(i)].center, nodes_[size_t(g)].center); };
    gScore[size_t(s)] = 0;
    open.push({h(s), s});
    int expansions = 0;
    while (!open.empty() && expansions < 6000) {
        auto [f, cur] = open.top();
        open.pop();
        ++expansions;
        if (cur == g) break;
        if (f > gScore[size_t(cur)] + h(cur) + 1e-3f) continue;
        for (int nb : nodes_[size_t(cur)].links) {
            if (nodes_[size_t(nb)].blocked) continue;
            float ng = gScore[size_t(cur)] + Vector3Distance(nodes_[size_t(cur)].center, nodes_[size_t(nb)].center);
            if (ng < gScore[size_t(nb)]) {
                gScore[size_t(nb)] = ng;
                came[size_t(nb)] = cur;
                open.push({ng + h(nb), nb});
            }
        }
    }
    if (came[size_t(g)] < 0 && s != g) return out;
    for (int c = g; c >= 0; c = came[size_t(c)]) {
        out.push_back(c);
        if (c == s) break;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::vector<Vector3> Nav::Path(Vector3 from, Vector3 to) const {
    std::vector<Vector3> v;
    for (int i : AStar(Locate(from), Locate(to))) v.push_back(nodes_[size_t(i)].center);
    return v;
}

Vector3 Nav::NextWaypoint(Vector3 from, Vector3 to) const {
    int s = Locate(from), g = Locate(to);
    if (s < 0 || g < 0 || s == g || LineClear(s, g)) return to;
    std::vector<int> p = AStar(s, g);
    if (p.size() < 2) return to;
    // Skip ahead to the farthest node with a clear straight line.
    size_t pick = 1;
    for (size_t k = 2; k < p.size() && k < 8; ++k)
        if (LineClear(s, p[k])) pick = k;
        else break;
    return nodes_[size_t(p[pick])].center;
}

}  // namespace lk
