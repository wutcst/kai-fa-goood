#pragma once

#include <string>

namespace fireice {

// Resolve asset path: cwd first, then executable directory.
std::string resolveAssetPath(const std::string& path);

}  // namespace fireice
