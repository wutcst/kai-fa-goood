#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace fireice {

constexpr uint8_t MAX_PICKUPS = 96;

struct Pickup {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    uint8_t index = 0;
};

std::vector<Pickup> loadPickupsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
void collectPickups(PlayerState& player, const std::vector<Pickup>& pickups, uint32_t& collectedMask,
                    uint32_t& collectedMaskHi, uint32_t& collectedMaskExt);

}  // namespace fireice
