#pragma once

#include "Map.hpp"
#include "Types.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace fireice {

constexpr float VANISHING_HIDE_DELAY = 0.65f;
constexpr float VANISHING_RESPAWN_TIME = 5.0f;
constexpr float MUD_SPAWN_INTERVAL = 2.2f;
constexpr float MUD_FALL_SPEED = 140.0f;
constexpr float MUD_HITBOX = 14.0f;
constexpr float FAN_RISE_SPEED = 480.0f;
constexpr int FAN_WIND_TILES_UP = 18;

struct FanZone {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float targetFeetY = 0.0f;
};

struct MudParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vy = MUD_FALL_SPEED;
    bool active = false;
};

struct LevelRuntime {
    std::vector<std::pair<int, int>> vanishingCoords;
    std::vector<float> vanishingHideTimer;
    std::vector<float> vanishingRespawnTimer;
    std::vector<Vec2> mudSpawners;
    std::vector<FanZone> fanZones;
    std::vector<std::pair<int, int>> fanTileCoords;
    std::array<MudParticle, MAX_MUD_PARTICLES> mudParticles{};
    float mudSpawnTimer = 0.0f;
    bool collectVictory = false;
    int nextMudSpawnIndex = 0;
};

void initLevelRuntime(const GameMap& map, LevelRuntime& runtime);
void resetLevelRuntime(LevelRuntime& runtime);
void syncVanishingMask(const LevelRuntime& runtime, WorldState& world);

bool isVanishingTileHidden(const WorldState& world, uint16_t slot);
bool mapHasExitTiles(const GameMap& map);

void triggerVanishingForPlayer(const PlayerState& player, const GameMap& map, LevelRuntime& runtime);
void updateLevelMechanics(LevelRuntime& runtime, GameMap& map, WorldState& world, float dt);
bool sampleSpikeHazard(const GameMap& map, const PlayerState& player);
bool sampleMudHazard(const LevelRuntime& runtime, const PlayerState& player);

std::vector<Vec2> loadMudSpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
std::vector<FanZone> loadFanZonesFromTmx(const std::string& tmxPath, int tmxTileWidth = 16,
                                         std::vector<std::pair<int, int>>* outFanTileCoords = nullptr);

}  // namespace fireice
