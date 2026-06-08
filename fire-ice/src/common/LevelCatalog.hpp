#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fireice {

struct LevelInfo {
    uint8_t id = 0;
    const char* fileName = "";
    const char* visualFileName = nullptr;
    const char* title = "";
    const char* subtitle = "";
};

class LevelCatalog {
public:
    static const LevelCatalog& instance();

    uint8_t count() const { return static_cast<uint8_t>(levels_.size()); }
    const LevelInfo& at(uint8_t index) const;
    std::string resolvePath(uint8_t index) const;
    std::string resolvePathByFile(const char* fileName) const;
    std::string resolveVisualPath(uint8_t index) const;

private:
    LevelCatalog();
    std::vector<LevelInfo> levels_;
};

} // namespace fireice
