#pragma once
// --validate: no window. Parses every glTF with cgltf and checks every data file against its
// expected fields (and anim maps against the clip names inside their models).
#include <string>

namespace lk {

// Returns the number of problems found (0 = OK). Logs each problem.
int ValidateContent();

}  // namespace lk
