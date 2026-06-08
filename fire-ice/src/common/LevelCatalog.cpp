#include "LevelCatalog.hpp"
#include "Paths.hpp"

namespace fireice {

const LevelCatalog& LevelCatalog::instance() {
    static LevelCatalog catalog;
    return catalog;
}

LevelCatalog::LevelCatalog() {
    levels_ = {
        {1, "level01_forest_entrance.txt", "Forest Entrance", "Tutorial - gems and exits"},
        {2, "level02_twin_pools.txt", "Twin Pools", "Separate lava and water paths"},
        {3, "level03_temple_gates.txt", "Temple Gates", "Buttons open elemental doors"},
        {4, "level04_gem_grotto.txt", "Gem Grotto", "Collect every diamond"},
        {5, "level05_vertical_shaft.txt", "Vertical Shaft", "Climb up together"},
        {6, "level06_coop_bridge.txt", "Co-op Bridge", "Help each other cross"},
        {7, "level07_element_maze.txt", "Element Maze", "Classic hazard maze"},
        {8, "level08_forest_shrine.txt", "Forest Shrine", "Final shrine challenge"},
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

} // namespace fireice
