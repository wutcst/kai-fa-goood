#include "LevelMechanics.hpp"

#include "Paths.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace fireice {

namespace {

int vanishingSlotForTile(const LevelRuntime& runtime, int tx, int ty) {
    for (std::size_t i = 0; i < runtime.vanishingCoords.size(); ++i) {
        if (runtime.vanishingCoords[i].first == tx && runtime.vanishingCoords[i].second == ty) {
            return static_cast<int>(i);
        }
    }
    return -1;
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
        const std::string needle = "<object";
        std::size_t inner = 0;
        while (true) {
            inner = groupBlock.find(needle, inner);
            if (inner == std::string::npos) {
                break;
            }
            const std::size_t end = groupBlock.find("/>", inner);
            const std::size_t endClose = groupBlock.find("</object>", inner);
            std::size_t tagEnd = std::string::npos;
            if (end != std::string::npos && (endClose == std::string::npos || end < endClose)) {
                tagEnd = end + 2;
            } else if (endClose != std::string::npos) {
                tagEnd = endClose + 9;
            }
            if (tagEnd == std::string::npos) {
                break;
            }
            tags.push_back(groupBlock.substr(inner, tagEnd - inner));
            inner = tagEnd;
        }
        pos = groupEnd + 14;
    }
    return tags;
}

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

}  // namespace

void initLevelRuntime(const GameMap& map, LevelRuntime& runtime) {
    runtime.vanishingCoords.clear();
    runtime.vanishingHideTimer.clear();
    runtime.vanishingRespawnTimer.clear();
    runtime.collectVictory = !mapHasExitTiles(map);

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (map.tileAt(x, y) != TileType::VanishingPlatform) {
                continue;
            }
            if (runtime.vanishingCoords.size() >= MAX_VANISHING_SLOTS) {
                goto done_vanishing_scan;
            }
            runtime.vanishingCoords.push_back({x, y});
            runtime.vanishingHideTimer.push_back(0.0f);
            runtime.vanishingRespawnTimer.push_back(0.0f);
        }
    }
done_vanishing_scan:
    (void) 0;
}

void triggerVanishingForPlayer(const PlayerState& player, const GameMap& map, LevelRuntime& runtime) {
    if (!player.alive || !player.onGround) {
        return;
    }

    const int tx = static_cast<int>(std::floor((player.x + PLAYER_WIDTH * 0.5f) / TILE_SIZE));
    const float feet = player.y + PLAYER_HEIGHT;
    const int tyCenter = static_cast<int>(std::floor(feet / TILE_SIZE));

    for (int ty = tyCenter; ty >= tyCenter - 1 && ty >= 0; --ty) {
        bool onFanTile = false;
        for (const std::pair<int, int>& fanTile : runtime.fanTileCoords) {
            if (fanTile.first == tx && fanTile.second == ty) {
                onFanTile = true;
                break;
            }
        }
        if (onFanTile) {
            continue;
        }
        if (map.tileAt(tx, ty) != TileType::VanishingPlatform) {
            continue;
        }
        const float tileTop = static_cast<float>(ty) * TILE_SIZE;
        if (feet < tileTop - 4.0f || feet > tileTop + 8.0f) {
            continue;
        }
        const int slot = vanishingSlotForTile(runtime, tx, ty);
        if (slot < 0) {
            continue;
        }
        if (runtime.vanishingRespawnTimer[static_cast<std::size_t>(slot)] > 0.0f) {
            continue;
        }
        if (runtime.vanishingHideTimer[static_cast<std::size_t>(slot)] > 0.0f) {
            continue;
        }
        runtime.vanishingHideTimer[static_cast<std::size_t>(slot)] = VANISHING_HIDE_DELAY;
        return;
    }
}

void resetLevelRuntime(LevelRuntime& runtime) {
    std::fill(runtime.vanishingHideTimer.begin(), runtime.vanishingHideTimer.end(), 0.0f);
    std::fill(runtime.vanishingRespawnTimer.begin(), runtime.vanishingRespawnTimer.end(), 0.0f);
    runtime.mudSpawnTimer = 0.0f;
    runtime.nextMudSpawnIndex = 0;
    for (MudParticle& particle : runtime.mudParticles) {
        particle.active = false;
    }
}

void syncVanishingMask(const LevelRuntime& runtime, WorldState& world) {
    world.vanishingCount =
        static_cast<uint16_t>(std::min(runtime.vanishingCoords.size(), static_cast<std::size_t>(MAX_VANISHING_SLOTS)));
    for (int i = 0; i < 9; ++i) {
        world.vanishingHidden[i] = 0;
    }
    for (std::size_t i = 0; i < runtime.vanishingCoords.size() && i < MAX_VANISHING_SLOTS; ++i) {
        if (runtime.vanishingRespawnTimer[i] > 0.0f) {
            world.vanishingHidden[i / 32] |= (1u << (i % 32));
        }
    }
}

bool isVanishingTileHidden(const WorldState& world, uint16_t slot) {
    if (slot >= world.vanishingCount) {
        return false;
    }
    return (world.vanishingHidden[slot / 32] & (1u << (slot % 32))) != 0;
}

bool mapHasExitTiles(const GameMap& map) {
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const TileType type = map.tileAt(x, y);
            if (type == TileType::FireExit || type == TileType::WaterExit || type == TileType::PoisonExit) {
                return true;
            }
        }
    }
    return false;
}

void updateLevelMechanics(LevelRuntime& runtime, GameMap& map, WorldState& world, float dt) {
    for (std::size_t i = 0; i < runtime.vanishingCoords.size(); ++i) {
        if (runtime.vanishingHideTimer[i] > 0.0f) {
            runtime.vanishingHideTimer[i] = std::max(0.0f, runtime.vanishingHideTimer[i] - dt);
            if (runtime.vanishingHideTimer[i] <= 0.0f) {
                runtime.vanishingRespawnTimer[i] = VANISHING_RESPAWN_TIME;
            }
        } else if (runtime.vanishingRespawnTimer[i] > 0.0f) {
            runtime.vanishingRespawnTimer[i] = std::max(0.0f, runtime.vanishingRespawnTimer[i] - dt);
        }
    }

    if (!runtime.mudSpawners.empty()) {
        runtime.mudSpawnTimer += dt;
        if (runtime.mudSpawnTimer >= MUD_SPAWN_INTERVAL) {
            runtime.mudSpawnTimer = 0.0f;
            const Vec2& spawn = runtime.mudSpawners[static_cast<std::size_t>(
                runtime.nextMudSpawnIndex % static_cast<int>(runtime.mudSpawners.size()))];
            runtime.nextMudSpawnIndex = (runtime.nextMudSpawnIndex + 1) % static_cast<int>(runtime.mudSpawners.size());

            for (MudParticle& particle : runtime.mudParticles) {
                if (particle.active) {
                    continue;
                }
                particle.active = true;
                particle.x = spawn.x;
                particle.y = spawn.y;
                particle.vy = MUD_FALL_SPEED;
                break;
            }
        }
    }

    const float mapBottom = static_cast<float>(map.height()) * TILE_SIZE + TILE_SIZE;
    for (MudParticle& particle : runtime.mudParticles) {
        if (!particle.active) {
            continue;
        }
        particle.y += particle.vy * dt;
        if (particle.y > mapBottom) {
            particle.active = false;
        }
    }

    world.mudParticleCount = 0;
    for (const MudParticle& particle : runtime.mudParticles) {
        if (!particle.active || world.mudParticleCount >= MAX_MUD_PARTICLES) {
            continue;
        }
        world.mudParticles[world.mudParticleCount].x = particle.x;
        world.mudParticles[world.mudParticleCount].y = particle.y;
        world.mudParticles[world.mudParticleCount].active = 1;
        ++world.mudParticleCount;
    }

    syncVanishingMask(runtime, world);
    (void) map;
}

bool sampleSpikeHazard(const GameMap& map, const PlayerState& player) {
    if (!player.alive) {
        return false;
    }

    const AABB box = playerBounds(player);
    const int minX = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((box.right() - 0.01f) / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((box.bottom() - 0.01f) / TILE_SIZE));

    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (map.tileAt(tx, ty) != TileType::Spike) {
                continue;
            }
            const AABB tileBox{tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            if (box.intersects(tileBox)) {
                return true;
            }
        }
    }
    return false;
}

bool sampleMudHazard(const LevelRuntime& runtime, const PlayerState& player) {
    if (!player.alive) {
        return false;
    }

    const AABB box = playerBounds(player);
    for (const MudParticle& particle : runtime.mudParticles) {
        if (!particle.active) {
            continue;
        }
        const AABB mudBox{particle.x - MUD_HITBOX * 0.5f, particle.y - MUD_HITBOX * 0.5f, MUD_HITBOX, MUD_HITBOX};
        if (box.intersects(mudBox)) {
            return true;
        }
    }
    return false;
}

std::vector<Vec2> loadMudSpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    std::vector<Vec2> spawns;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return spawns;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const float scale = TILE_SIZE / static_cast<float>(std::max(1, tmxTileWidth));

    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        if (tag.find("gid=\"303\"") == std::string::npos) {
            continue;
        }
        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float oh = attrFloat(tag, "height", static_cast<float>(tmxTileWidth));
        spawns.push_back({ox * scale, (oy - oh * 0.5f) * scale});
    }

    return spawns;
}

std::vector<FanZone> loadFanZonesFromTmx(const std::string& tmxPath, int tmxTileWidth,
                                         std::vector<std::pair<int, int>>* outFanTileCoords) {
    std::vector<FanZone> zones;
    if (outFanTileCoords != nullptr) {
        outFanTileCoords->clear();
    }
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return zones;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();

    int mapWidth = 0;
    const std::string widthKey = "width=\"";
    const std::size_t widthPos = xml.find(widthKey);
    if (widthPos != std::string::npos) {
        mapWidth = std::stoi(xml.substr(widthPos + widthKey.size()));
    }

    constexpr int kFanFirstGid = 244;
    constexpr int kFanLastGid = 247;

    const std::string dataOpen = "<data encoding=\"csv\">";
    const std::size_t dataPos = xml.find(dataOpen);
    if (dataPos == std::string::npos) {
        return zones;
    }
    const std::size_t dataStart = dataPos + dataOpen.size();
    const std::size_t dataEnd = xml.find("</data>", dataStart);
    if (dataEnd == std::string::npos) {
        return zones;
    }

    const std::string csv = xml.substr(dataStart, dataEnd - dataStart);
    std::vector<int> gids;
    std::string token;
    for (char c : csv) {
        if (c == ',' || c == '\n' || c == '\r') {
            if (!token.empty()) {
                gids.push_back(std::stoi(token));
                token.clear();
            }
        } else if (c != ' ' && c != '\t') {
            token.push_back(c);
        }
    }
    if (!token.empty()) {
        gids.push_back(std::stoi(token));
    }

    if (mapWidth <= 0 || static_cast<int>(gids.size()) < mapWidth) {
        return zones;
    }

    const int mapHeight = static_cast<int>(gids.size()) / mapWidth;
    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            const int rawGid = gids[static_cast<std::size_t>(y * mapWidth + x)];
            const int gid = rawGid & 0x1FFFFFFF;
            if (gid < kFanFirstGid || gid > kFanLastGid) {
                continue;
            }

            if (outFanTileCoords != nullptr) {
                outFanTileCoords->push_back({x, y});
            }

            FanZone zone;
            zone.left = static_cast<float>(x) * TILE_SIZE - TILE_SIZE * 0.5f;
            zone.width = TILE_SIZE * 2.0f;
            const float bottom = static_cast<float>(y + 2) * TILE_SIZE;
            const float topY = static_cast<float>(std::max(0, y - FAN_WIND_TILES_UP)) * TILE_SIZE;
            zone.top = topY;
            zone.height = bottom - topY;
            const int brickRow = x < mapWidth / 2 ? 19 : 20;
            zone.targetFeetY = static_cast<float>(brickRow) * TILE_SIZE - PLAYER_HEIGHT;
            zones.push_back(zone);
        }
    }

    (void) tmxTileWidth;
    return zones;
}

}  // namespace fireice
