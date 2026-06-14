#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fireice {

struct LevelInfo {
    uint8_t id = 0;
    const char* fileName = "";           // 服务端碰撞：assets/levels/*.txt
    const char* visualFileName = nullptr; // 客户端 Tiled：assets/maps/*.tmx
    const char* title = "";
    const char* subtitle = "";
    uint8_t minPlayers = 1;
    uint8_t maxPlayers = 2;
};

class LevelCatalog {
public:
    static const LevelCatalog& instance();

    uint8_t count() const { return static_cast<uint8_t>(levels_.size()); }
    uint8_t countForPlayerCount(uint8_t playerCount) const;
    // 全局关卡序号 ↔ 按人数过滤后的选关列表序号
    uint8_t globalIndexToFilteredIndex(uint8_t globalIndex, uint8_t playerCount) const;
    uint8_t filteredIndexToGlobalIndex(uint8_t filteredIndex, uint8_t playerCount) const;
    const LevelInfo& at(uint8_t index) const;
    std::string resolvePath(uint8_t index) const;
    std::string resolvePathByFile(const char* fileName) const;
    std::string resolveVisualPath(uint8_t index) const;

private:
    LevelCatalog();
    std::vector<LevelInfo> levels_;
};

}  // namespace fireice
