#pragma once

#include <string>

namespace fireice {

// Resolve asset path: executable directory first, then cwd.
std::string resolveAssetPath(const std::string& path);

}  // namespace fireice
