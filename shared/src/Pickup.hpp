#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace fireice {

class GameMap;

constexpr uint8_t MAX_PICKUPS = 96;

enum class PickupKind : uint8_t { Fruit = 0, Magnet = 1 };

struct Pickup {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    uint8_t index = 0;
    PickupKind kind = PickupKind::Fruit;
};

struct TmxTilesetInfo {
    int firstGid = 0;
    int tileCount = 0;
    std::string source;
    bool isCollectible = false;
};

bool isCollectibleTilesetSource(const std::string& source);
bool isSawTilesetSource(const std::string& source);
bool isRockHeadTilesetSource(const std::string& source);
bool isSpikedBallTilesetSource(const std::string& source);
bool isChainTilesetSource(const std::string& source);
std::vector<TmxTilesetInfo> loadTmxTilesetInfo(const std::string& tmxPath);
bool isCollectibleGid(int gid, const std::vector<TmxTilesetInfo>& tilesets);
bool isSawGid(int gid, const std::vector<TmxTilesetInfo>& tilesets);
bool isRockHeadGid(int gid, const std::vector<TmxTilesetInfo>& tilesets);
bool isSpikedBallGid(int gid, const std::vector<TmxTilesetInfo>& tilesets);
bool isChainGid(int gid, const std::vector<TmxTilesetInfo>& tilesets);

std::vector<Pickup> loadPickupsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
void collectPickups(PlayerState& player, const std::vector<Pickup>& pickups, uint32_t& collectedMask,
                    uint32_t& collectedMaskHi, uint32_t& collectedMaskExt);

}  // namespace fireice
