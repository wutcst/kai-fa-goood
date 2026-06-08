#include "LevelCatalog.hpp"
#include "Paths.hpp"

namespace fireice {

const LevelCatalog& LevelCatalog::instance() {
    static LevelCatalog catalog;
    return catalog;
}

LevelCatalog::LevelCatalog() {
    levels_ = {
        {1, "level01_forest_entrance.txt", "Forest Entrance", "Tutorial - gems and exits", 1, 2},
        {2, "level02_twin_pools.txt", "Twin Pools", "Separate lava and water paths", 1, 2},
        {3, "level03_temple_gates.txt", "Temple Gates", "Buttons open elemental doors", 2, 2},
        {4, "level04_gem_grotto.txt", "Gem Grotto", "Collect every diamond", 2, 2},
        {5, "level05_vertical_shaft.txt", "Vertical Shaft", "Climb up together", 1, 3},
        {6, "level06_coop_bridge.txt", "Co-op Bridge", "Help each other cross", 1, 3},
        {7, "level07_element_maze.txt", "Element Maze", "Classic hazard maze", 1, 3},
        {8, "level08_forest_shrine.txt", "Forest Shrine", "Final shrine challenge", 2, 3},
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
