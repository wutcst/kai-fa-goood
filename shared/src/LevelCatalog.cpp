#include "LevelCatalog.hpp"
#include "Paths.hpp"

namespace fireice {

const LevelCatalog& LevelCatalog::instance() {
    static LevelCatalog catalog;
    return catalog;
}

LevelCatalog::LevelCatalog() {
    // fileName = server collision; visualFileName = client Tiled map.
    // Game "Level N" uses levelNN assets (only entries here are playable):
    //   1 level01  Forest Entrance   brown forest, platforms, strawberries
    //   2 level02  Banana Temple     vanishing platforms, mud, fans
    //   3 level03  Temple Gates      elemental doors and buttons
    //   4 level04  Gem Grotto        collect every gem
    //   5 level05  Vertical Shaft    climb upward
    //   6 level06  Co-op Bridge      cooperation required
    //   7 level07  Element Maze      lava / water zones
    //   8 level08  Forest Shrine     final shrine
    levels_ = {
        {1, "level01_collision.txt", "level01.tmx", "Forest Entrance", "Learn the basics", 1, 3},
        {2, "level02_collision.txt", "level02.tmx", "Banana Temple",
         "Vanishing platforms, mud traps and flying guardians", 1, 3},
        {3, "level03_collision.txt", "level03.tmx", "Temple Gates", "Buttons open elemental doors", 1, 3},
        {4, "level04_collision.txt", "level04.tmx", "Gem Grotto", "Collect every diamond", 1, 3},
        {5, "level05_collision.txt", "level05.tmx", "Vertical Shaft", "Climb up together", 1, 3},
        {6, "level06_collision.txt", "level06.tmx", "Co-op Bridge", "Help each other cross", 1, 3},
        {7, "level07_collision.txt", "level07.tmx", "Element Maze", "Classic hazard maze", 1, 3},
        {8, "level08_collision.txt", "level08.tmx", "Forest Shrine", "Final shrine challenge", 1, 3},
    };
}

const LevelInfo& LevelCatalog::at(uint8_t index) const {
    if (index >= levels_.size()) {
        return levels_.front();
    }
    return levels_[index];
}

std::string LevelCatalog::resolvePath(uint8_t index) const {
    return resolvePathByFile(at(index).fileName);
}

std::string LevelCatalog::resolvePathByFile(const char* fileName) const {
    return resolveAssetPath(std::string("levels/") + fileName);
}

std::string LevelCatalog::resolveVisualPath(uint8_t index) const {
    const LevelInfo& info = at(index);
    if (info.visualFileName == nullptr || info.visualFileName[0] == '\0') {
        return {};
    }
    return resolveAssetPath(std::string("maps/") + info.visualFileName);
}

uint8_t LevelCatalog::countForPlayerCount(uint8_t playerCount) const {
    uint8_t count = 0;
    for (const LevelInfo& info : levels_) {
        if (playerCount >= info.minPlayers && playerCount <= info.maxPlayers) {
            ++count;
        }
    }
    return count;
}

uint8_t LevelCatalog::globalIndexToFilteredIndex(uint8_t globalIndex, uint8_t playerCount) const {
    // 统计 globalIndex 之前有多少关符合当前人数
    uint8_t filtered = 0;
    for (uint8_t i = 0; i < globalIndex && i < levels_.size(); ++i) {
        if (playerCount >= levels_[i].minPlayers && playerCount <= levels_[i].maxPlayers) {
            ++filtered;
        }
    }
    return filtered;
}

uint8_t LevelCatalog::filteredIndexToGlobalIndex(uint8_t filteredIndex, uint8_t playerCount) const {
    uint8_t filtered = 0;
    for (uint8_t i = 0; i < levels_.size(); ++i) {
        if (playerCount >= levels_[i].minPlayers && playerCount <= levels_[i].maxPlayers) {
            if (filtered == filteredIndex) {
                return i;
            }
            ++filtered;
        }
    }
    return 0;
}

}  // namespace fireice
