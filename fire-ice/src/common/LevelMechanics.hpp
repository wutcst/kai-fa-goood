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

constexpr float SAW_DEFAULT_TRAVEL_TMX = 80.0f;
constexpr float SAW_DEFAULT_SPEED = 2.4f;
constexpr float SAW_HITBOX_SCALE = 0.82f;
constexpr uint8_t MAX_SAW_TRAPS = 16;

constexpr float ROCK_HEAD_SPEED = 260.0f;
constexpr float ROCK_HEAD_WAIT_TIME = 1.0f;
constexpr float ROCK_HEAD_HITBOX_SCALE = 0.92f;

constexpr float PENDULUM_MAX_ANGLE = 0.55f;
constexpr float PENDULUM_SPEED = 2.2f;
constexpr float PENDULUM_BALL_HITBOX_SCALE = 0.86f;

struct PendulumTrap {
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float chainLength = 0.0f;
    float ballW = 0.0f;
    float ballH = 0.0f;
    float phase = 0.0f;
    int ballGid = 0;
    int chainGid = 0;
    uint8_t chainCount = 3;
};

struct RockHeadTrap {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float baseX = 0.0f;
    float baseY = 0.0f;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    float waitTimer = 0.0f;
    int dirX = 0;
    int dirY = 0;
    int startDirX = 0;
    int startDirY = 0;
    int gid = 0;
};

struct SawTrap {
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float travelRange = 0.0f;
    float speed = SAW_DEFAULT_SPEED;
    float phase = 0.0f;
    int gid = 0;
};

struct LevelRuntime {
    std::vector<std::pair<int, int>> vanishingCoords;
    std::vector<float> vanishingHideTimer;
    std::vector<float> vanishingRespawnTimer;
    std::vector<Vec2> mudSpawners;
    std::vector<FanZone> fanZones;
    std::vector<std::pair<int, int>> fanTileCoords;
    std::vector<SawTrap> sawTraps;
    std::vector<RockHeadTrap> rockHeads;
    std::vector<PendulumTrap> pendulums;
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
bool sampleSawHazard(const std::vector<SawTrap>& saws, const PlayerState& player, float timeSec);
void updateRockHeads(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt);
void updatePendulums(const LevelRuntime& runtime, WorldState& world, float timeSec);
bool samplePendulumHazard(const std::vector<PendulumTrap>& pendulums, const PlayerState& player, float timeSec);
AABB rockHeadHitbox(const RockHeadTrap& rock);
std::vector<RockHeadTrap> loadRockHeadsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
std::vector<PendulumTrap> loadPendulumsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
float sawOffsetY(const SawTrap& saw, float timeSec);
float sawCurrentY(const SawTrap& saw, float timeSec);
AABB sawHitbox(const SawTrap& saw, float timeSec);

std::vector<SawTrap> loadSawTrapsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
std::vector<Vec2> loadMudSpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
std::vector<FanZone> loadFanZonesFromTmx(const std::string& tmxPath, int tmxTileWidth = 16,
                                         std::vector<std::pair<int, int>>* outFanTileCoords = nullptr);

}  // namespace fireice
