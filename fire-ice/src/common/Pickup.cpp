#include "Pickup.hpp"

#include "Paths.hpp"
#include "Physics.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace fireice {

namespace {

constexpr float kPickupHitboxScale = 0.88f;

float attrFloat(const std::string& tag, const char* name, float fallback) {
    const std::string key = std::string(name) + "=\"";
    const std::size_t pos = tag.find(key);
    if (pos == std::string::npos) {
        return fallback;
    }
    const std::size_t start = pos + key.size();
    const std::size_t end = tag.find('"', start);
    if (end == std::string::npos) {
        return fallback;
    }
    return std::stof(tag.substr(start, end - start));
}

bool hasAttr(const std::string& tag, const char* name, const char* value) {
    const std::string key = std::string(name) + "=\"";
    const std::size_t pos = tag.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    const std::size_t start = pos + key.size();
    const std::size_t end = tag.find('"', start);
    if (end == std::string::npos) {
        return false;
    }
    return tag.substr(start, end - start) == value;
}

bool isSpawnName(const std::string& tag) {
    if (hasAttr(tag, "name", "player1") || hasAttr(tag, "name", "player2") || hasAttr(tag, "name", "player")) {
        return true;
    }
    return false;
}

std::vector<std::string> findObjectTags(const std::string& xml) {
    std::vector<std::string> tags;
    const std::string needle = "<object";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(needle, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t end = xml.find("/>", pos);
        const std::size_t endClose = xml.find("</object>", pos);
        std::size_t tagEnd = std::string::npos;
        if (end != std::string::npos && (endClose == std::string::npos || end < endClose)) {
            tagEnd = end + 2;
        } else if (endClose != std::string::npos) {
            tagEnd = endClose + 9;
        }
        if (tagEnd == std::string::npos) {
            break;
        }
        tags.push_back(xml.substr(pos, tagEnd - pos));
        pos = tagEnd;
    }
    return tags;
}

std::vector<std::string> findObjectTagsInGroups(const std::string& xml) {
    std::vector<std::string> tags;
    const std::string groupOpen = "<objectgroup";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(groupOpen, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t groupEnd = xml.find("</objectgroup>", pos);
        if (groupEnd == std::string::npos) {
            break;
        }
        const std::string groupBlock = xml.substr(pos, groupEnd - pos);
        for (const std::string& tag : findObjectTags(groupBlock)) {
            tags.push_back(tag);
        }
        pos = groupEnd + 14;
    }
    return tags;
}

}  // namespace

std::vector<Pickup> loadPickupsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    std::vector<Pickup> pickups;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return pickups;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const float scale = TILE_SIZE / static_cast<float>(std::max(1, tmxTileWidth));

    uint8_t index = 0;
    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        if (tag.find("gid=\"") == std::string::npos || isSpawnName(tag)) {
            continue;
        }
        if (tag.find("gid=\"303\"") != std::string::npos) {
            continue;
        }
        if (index >= MAX_PICKUPS) {
            break;
        }

        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float ow = attrFloat(tag, "width", static_cast<float>(tmxTileWidth));
        const float oh = attrFloat(tag, "height", static_cast<float>(tmxTileWidth));

        Pickup pickup;
        pickup.x = ox * scale;
        pickup.y = (oy - oh) * scale;
        pickup.w = ow * scale;
        pickup.h = oh * scale;
        pickup.index = index++;
        pickups.push_back(pickup);
    }

    return pickups;
}

void collectPickups(PlayerState& player, const std::vector<Pickup>& pickups, uint32_t& collectedMask,
                    uint32_t& collectedMaskHi, uint32_t& collectedMaskExt) {
    if (!player.alive || pickups.empty()) {
        return;
    }

    const AABB box = playerCollectBounds(player);
    for (const Pickup& pickup : pickups) {
        const uint32_t bit = 1u << (pickup.index % 32u);
        const uint8_t word = pickup.index / 32u;
        if (word > 2) {
            continue;
        }
        const uint32_t maskWord = word == 0 ? collectedMask : (word == 1 ? collectedMaskHi : collectedMaskExt);
        if (maskWord & bit) {
            continue;
        }

        const float hitW = pickup.w * kPickupHitboxScale;
        const float hitH = pickup.h * kPickupHitboxScale;
        const float hitX = pickup.x + (pickup.w - hitW) * 0.5f;
        const float hitY = pickup.y + (pickup.h - hitH) * 0.5f;
        const AABB pickupBox{hitX, hitY, hitW, hitH};
        if (!box.intersects(pickupBox)) {
            continue;
        }

        if (word == 0) {
            collectedMask |= bit;
        } else if (word == 1) {
            collectedMaskHi |= bit;
        } else {
            collectedMaskExt |= bit;
        }
        ++player.gems;
    }
}

}  // namespace fireice
