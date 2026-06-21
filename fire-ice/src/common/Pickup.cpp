#include "Pickup.hpp"

#include "Paths.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
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

int attrInt(const std::string& tag, const char* name, int fallback) {
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
    return std::stoi(tag.substr(start, end - start));
}

std::optional<int> xmlAttrInt(const std::string& tag, const char* name) {
    const std::string key = std::string(name) + "=\"";
    const std::size_t pos = tag.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t start = pos + key.size();
    const std::size_t end = tag.find('"', start);
    if (end == std::string::npos) {
        return std::nullopt;
    }
    return std::stoi(tag.substr(start, end - start));
}

std::optional<int> jsonIntField(const std::string& json, const char* key) {
    const std::string keyToken = std::string("\"") + key + "\":";
    const std::size_t start = json.find(keyToken);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    std::size_t valueStart = start + keyToken.size();
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
        ++valueStart;
    }
    std::size_t valueEnd = valueStart;
    while (valueEnd < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[valueEnd])) || json[valueEnd] == '-')) {
        ++valueEnd;
    }
    if (valueEnd == valueStart) {
        return std::nullopt;
    }
    return std::stoi(json.substr(valueStart, valueEnd - valueStart));
}

int readTileCountFromTilesetFile(const std::string& tilesetPath) {
    std::ifstream file(tilesetPath);
    if (!file.is_open()) {
        return 0;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();
    if (tilesetPath.size() >= 4 && tilesetPath.substr(tilesetPath.size() - 4) == ".tsj") {
        if (const auto count = jsonIntField(content, "tilecount")) {
            return *count;
        }
        return 0;
    }
    const std::string needle = "<tileset";
    const std::size_t pos = content.find(needle);
    if (pos == std::string::npos) {
        return 0;
    }
    const std::size_t end = content.find('>', pos);
    if (end == std::string::npos) {
        return 0;
    }
    const std::string tag = content.substr(pos, end - pos + 1);
    if (const auto count = xmlAttrInt(tag, "tilecount")) {
        return *count;
    }
    return 0;
}

std::vector<std::string> findTilesetTags(const std::string& xml) {
    std::vector<std::string> tags;
    const std::string needle = "<tileset";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(needle, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t end = xml.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        tags.push_back(xml.substr(pos, end - pos + 1));
        pos = end + 1;
    }
    return tags;
}

}  // namespace

bool isCollectibleTilesetSource(const std::string& source) {
    std::string lower = source;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("kiwi") != std::string::npos || lower.find("apple") != std::string::npos ||
           lower.find("banana") != std::string::npos || lower.find("cherry") != std::string::npos ||
           lower.find("orange") != std::string::npos || lower.find("melon") != std::string::npos ||
           lower.find("pineapple") != std::string::npos || lower.find("strawber") != std::string::npos ||
           lower.find("fruits/") != std::string::npos;
}

bool isSawTilesetSource(const std::string& source) {
    std::string lower = source;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("on (38x38)") != std::string::npos || lower.find("saw/") != std::string::npos;
}

bool isRockHeadTilesetSource(const std::string& source) {
    std::string lower = source;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("rock head") != std::string::npos || lower.find("rock_head") != std::string::npos;
}

bool isSpikedBallTilesetSource(const std::string& source) {
    std::string lower = source;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("spiked ball") != std::string::npos;
}

bool isChainTilesetSource(const std::string& source) {
    std::string lower = source;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("chain") != std::string::npos;
}
std::vector<TmxTilesetInfo> loadTmxTilesetInfo(const std::string& tmxPath) {
    std::vector<TmxTilesetInfo> tilesets;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return tilesets;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const std::filesystem::path mapDir = std::filesystem::path(path).parent_path();

    for (const std::string& tag : findTilesetTags(xml)) {
        const auto firstGid = xmlAttrInt(tag, "firstgid");
        const std::string key = "source=\"";
        const std::size_t sourcePos = tag.find(key);
        if (sourcePos == std::string::npos || !firstGid) {
            continue;
        }
        const std::size_t sourceStart = sourcePos + key.size();
        const std::size_t sourceEnd = tag.find('"', sourceStart);
        if (sourceEnd == std::string::npos) {
            continue;
        }

        TmxTilesetInfo info;
        info.firstGid = *firstGid;
        info.source = tag.substr(sourceStart, sourceEnd - sourceStart);
        info.isCollectible = isCollectibleTilesetSource(info.source);
        const std::string tilesetPath = resolveAssetPath((mapDir / info.source).generic_string());
        info.tileCount = readTileCountFromTilesetFile(tilesetPath);
        tilesets.push_back(std::move(info));
    }

    std::sort(tilesets.begin(), tilesets.end(),
              [](const TmxTilesetInfo& a, const TmxTilesetInfo& b) { return a.firstGid < b.firstGid; });
    return tilesets;
}

bool isCollectibleGid(int gid, const std::vector<TmxTilesetInfo>& tilesets) {
    if (gid <= 0) {
        return false;
    }
    const TmxTilesetInfo* chosen = nullptr;
    for (const TmxTilesetInfo& tileset : tilesets) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    if (chosen == nullptr || !chosen->isCollectible) {
        return false;
    }
    if (chosen->tileCount > 0 && gid >= chosen->firstGid + chosen->tileCount) {
        return false;
    }
    return true;
}

bool isSawGid(int gid, const std::vector<TmxTilesetInfo>& tilesets) {
    if (gid <= 0) {
        return false;
    }
    const TmxTilesetInfo* chosen = nullptr;
    for (const TmxTilesetInfo& tileset : tilesets) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    if (chosen == nullptr || !isSawTilesetSource(chosen->source)) {
        return false;
    }
    if (chosen->tileCount > 0 && gid >= chosen->firstGid + chosen->tileCount) {
        return false;
    }
    return true;
}

bool isRockHeadGid(int gid, const std::vector<TmxTilesetInfo>& tilesets) {
    if (gid <= 0) {
        return false;
    }
    const TmxTilesetInfo* chosen = nullptr;
    for (const TmxTilesetInfo& tileset : tilesets) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    if (chosen == nullptr || !isRockHeadTilesetSource(chosen->source)) {
        return false;
    }
    if (chosen->tileCount > 0 && gid >= chosen->firstGid + chosen->tileCount) {
        return false;
    }
    return true;
}

bool isSpikedBallGid(int gid, const std::vector<TmxTilesetInfo>& tilesets) {
    if (gid <= 0) {
        return false;
    }
    const TmxTilesetInfo* chosen = nullptr;
    for (const TmxTilesetInfo& tileset : tilesets) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    if (chosen == nullptr || !isSpikedBallTilesetSource(chosen->source)) {
        return false;
    }
    if (chosen->tileCount > 0 && gid >= chosen->firstGid + chosen->tileCount) {
        return false;
    }
    return true;
}

bool isChainGid(int gid, const std::vector<TmxTilesetInfo>& tilesets) {
    if (gid <= 0) {
        return false;
    }
    const TmxTilesetInfo* chosen = nullptr;
    for (const TmxTilesetInfo& tileset : tilesets) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    if (chosen == nullptr || !isChainTilesetSource(chosen->source)) {
        return false;
    }
    if (chosen->tileCount > 0 && gid >= chosen->firstGid + chosen->tileCount) {
        return false;
    }
    return true;
}
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
    const std::vector<TmxTilesetInfo> tilesets = loadTmxTilesetInfo(tmxPath);

    uint8_t index = 0;
    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        if (tag.find("gid=\"") == std::string::npos || isSpawnName(tag)) {
            continue;
        }
        const int gid = attrInt(tag, "gid", 0);
        if (!isCollectibleGid(gid, tilesets)) {
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
