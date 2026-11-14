#include "game/smoke_bot.h"

#include <cmath>

#include "engine/nav.h"
#include "game/session.h"

namespace lk {

InputFrame SmokeBot::MoveToward(Session& s, Vector3 target, float stopDist, bool& arrived) {
    InputFrame in;
    Vector3 p = s.PlayerPos();
    Vector3 to{target.x - p.x, 0, target.z - p.z};
    float d = std::sqrt(to.x * to.x + to.z * to.z);
    arrived = d < stopDist;
    if (arrived) return in;
    Vector3 next = s.GetNav().NextWaypoint(p, target);
    Vector3 dir = Vector3Normalize(Vector3{next.x - p.x, 0, next.z - p.z});
    Vector3 f = s.Camera().Forward(), r = s.Camera().Right();
    in.move = {Vector3DotProduct(dir, r), Vector3DotProduct(dir, f)};
    return in;
}

InputFrame SmokeBot::Think(Session& s, bool& done, bool& failed) {
    done = failed = false;
    ++t_;
    InputFrame in;
    switch (goal_) {
        case Goal::Kill: {
            if (killsAtStart_ < 0) killsAtStart_ = s.Kills();
            if (s.Kills() > killsAtStart_) {
                status_ = "killed an enemy";
                killsAtStart_ = -1;
                done = true;
                break;
            }
            Enemy* best = nullptr;
            float bestD = 1e9f;
            for (auto& e : s.Enemies())
                if (e->Alive() && e->Def().brain != "training") {
                    float d = Vector3Distance(e->Pos(), s.PlayerPos());
                    if (d < bestD) {
                        bestD = d;
                        best = e.get();
                    }
                }
            if (!best) {
                status_ = "no enemy to kill";
                failed = true;
                done = true;
                break;
            }
            bool arrived = false;
            in = MoveToward(s, best->Pos(), 1.1f, arrived);
            if (arrived) {
                if (s.LockTarget() != best) s.ForceLock(best->Id());
                if (t_ % 22 == 0) in.Press(Act::Attack);
            }
            if (t_ > 1500) {
                status_ = "timed out killing an enemy";
                failed = done = true;
            }
            break;
        }
        case Goal::Rest: {
            if (restsAtStart_ < 0) restsAtStart_ = s.RestCount();
            if (s.Shrines().empty()) {
                status_ = "no shrine";
                failed = done = true;
                break;
            }
            const Shrine& sh = s.Shrines().front();
            if (s.RestCount() > restsAtStart_) {
                // Leave the shrine menu.
                if (s.MenuOpen()) {
                    if (t_ % 10 == 0) in.Press(Act::Back);
                } else {
                    status_ = "rested at " + sh.id;
                    restsAtStart_ = -1;
                    done = true;
                }
                break;
            }
            bool arrived = false;
            in = MoveToward(s, sh.pos, 1.2f, arrived);
            if (arrived && t_ % 30 == 0) in.Press(Act::Interact);  // first press lights, second rests
            if (t_ > 1800) {
                status_ = "timed out resting";
                failed = done = true;
            }
            break;
        }
        case Goal::Wander: {
            const Nav& nav = s.GetNav();
            if (t_ == 1 || t_ % 400 == 0) {
                int n = nav.NodeCount();
                if (n > 0) {
                    wanderIndex_ = (wanderIndex_ * 7 + 13) % n;
                    wanderTarget_ = nav.NodeCenter(wanderIndex_);
                }
            }
            bool arrived = false;
            in = MoveToward(s, wanderTarget_, 0.8f, arrived);
            if (t_ % 90 == 45) in.Press(Act::Attack);
            if (t_ % 150 == 75) in.Press(Act::Roll);
            break;
        }
        case Goal::Done:
            break;
    }
    return in;
}

}  // namespace lk
