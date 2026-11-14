#pragma once
// Scripted player for --smoke-test: walks the nav graph, fights, lights and rests at a shrine.
// Produces InputFrames exactly like a pad would, so the real input path is exercised.
#include <string>
#include <vector>

#include "engine/input.h"
#include "raylib.h"

namespace lk {

class Session;
class Nav;

class SmokeBot {
public:
    enum class Goal { Kill, Rest, Wander, Done };
    void Start(Goal g) { goal_ = g; t_ = 0; phase_ = 0; }
    Goal Current() const { return goal_; }
    // Returns the input for this step. `done` is set when the goal completed (or timed out).
    InputFrame Think(Session& s, bool& done, bool& failed);
    std::string Status() const { return status_; }

private:
    InputFrame MoveToward(Session& s, Vector3 target, float stopDist, bool& arrived);
    Goal goal_ = Goal::Done;
    int t_ = 0, phase_ = 0, killsAtStart_ = -1, restsAtStart_ = -1;
    int wanderIndex_ = 0;
    Vector3 wanderTarget_{};
    std::string status_;
};

}  // namespace lk
