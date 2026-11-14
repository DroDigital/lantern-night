#include "game/combat.h"

namespace lk {

void SwingTracker::Capsules(Vector3 base, Vector3 tip, float radius, std::vector<geom::Capsule>& out) {
    out.clear();
    out.push_back(geom::Capsule{G(base), G(tip), radius});
    if (havePrev) {
        out.push_back(geom::Capsule{G(prevTip), G(tip), radius});
        Vector3 midPrev = Vector3Lerp(prevBase, prevTip, 0.5f), mid = Vector3Lerp(base, tip, 0.5f);
        out.push_back(geom::Capsule{G(midPrev), G(mid), radius});
    }
    prevBase = base;
    prevTip = tip;
    havePrev = true;
}

}  // namespace lk
