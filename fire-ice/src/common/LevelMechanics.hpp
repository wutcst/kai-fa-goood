#pragma once

#include "Map.hpp"
#include "Types.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace fireice {

struct Pickup;

constexpr float VANISHING_HIDE_DELAY = 0.65f;
constexpr float VANISHING_RESPAWN_TIME = 5.0f;
constexpr float MUD_SPAWN_INTERVAL = 2.2f;
constexpr float MUD_FALL_SPEED = 140.0f;
constexpr float MUD_HITBOX = 14.0f;
constexpr float FAN_RISE_SPEED = 980.0f;
constexpr float FAN_UP_ACCEL = 5200.0f;
constexpr int FAN_WIND_TILES_UP = 28;

struct FanZone {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float targetFeetY = 0.0f;
    float emitterX = 0.0f;
    float emitterY = 0.0f;
};

struct MudParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vy = MUD_FALL_SPEED;
    bool active = false;
};

struct MagnetDrop {
    float x = 0.0f;
    float y = 0.0f;
    float vy = 0.0f;
    float landedTimer = 0.0f;
    uint8_t kind = 0;
    bool active = false;
    bool falling = true;
};

enum class PowerUpKind : uint8_t { Magnet = 0, SpeedBoost = 1 };

struct PowerUpSpawn {
    float x = 0.0f;
    PowerUpKind kind = PowerUpKind::Magnet;
};

struct MagnetPull {
    float x = 0.0f;
    float y = 0.0f;
    float originX = 0.0f;
    float originY = 0.0f;
    bool active = false;
    bool isGem = false;
    int gemTx = 0;
    int gemTy = 0;
    uint8_t pickupIndex = 0;
};

constexpr float SAW_DEFAULT_TRAVEL_TMX = 80.0f;
constexpr float SAW_DEFAULT_SPEED = 2.4f;
constexpr float SAW_HITBOX_SCALE = 0.82f;

constexpr float ROCK_HEAD_SPEED = 260.0f;
constexpr float ROCK_HEAD_WAIT_TIME = 1.0f;
constexpr float ROCK_HEAD_HITBOX_SCALE = 0.92f;

constexpr float PENDULUM_MAX_ANGLE = 0.55f;
constexpr float PENDULUM_SPEED = 2.2f;
constexpr float PENDULUM_BALL_HITBOX_SCALE = 0.86f;

constexpr float ENEMY_PATROL_SPEED = 88.0f;
constexpr float ENEMY_PATROL_HALF_WIDTH = 208.0f;
constexpr float ENEMY_HITBOX_W = 42.0f;
constexpr float ENEMY_HITBOX_H = 34.0f;
constexpr float TRIDENT_SPEED = 280.0f;
constexpr float TRIDENT_SHOOT_INTERVAL = 2.35f;
constexpr float TRIDENT_HITBOX = 16.0f;
constexpr float ENEMY_INITIAL_SHOOT_DELAY = 1.2f;

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

struct FlyingEnemy {
    float x = 0.0f;
    float y = 0.0f;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    int8_t facing = 1;
    float shootTimer = ENEMY_INITIAL_SHOOT_DELAY;
    float wingAnimTimer = 0.0f;
    uint8_t wingPhase = 0;
    bool active = false;
};

struct TridentProjectile {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float rotation = 0.0f;
    bool active = false;
};

struct LevelRuntime {
    std::vector<std::pair<int, int>> vanishingCoords;
    std::vector<float> vanishingHideTimer;
    std::vector<float> vanishingRespawnTimer;
    std::vector<Vec2> mudSpawners;
    std::vector<PowerUpSpawn> powerUpSpawns;
    std::array<MagnetDrop, MAX_MAGNET_DROPS> magnetDrops{};
    std::array<MagnetPull, MAX_MAGNET_PULLS> magnetPulls{};
    uint8_t magnetSpawnsRemaining = 0;
    float magnetSpawnTimer = 0.0f;
    float powerUpLevelTimer = 0.0f;
    uint8_t nextPowerUpScheduleIndex = 0;
    int nextMagnetSpawnIndex = 0;
    int nextSpeedSpawnIndex = 0;
    bool magnetSpawnInitialized = false;
    std::vector<FanZone> fanZones;
    std::vector<std::pair<int, int>> fanTileCoords;
    std::vector<SawTrap> sawTraps;
    std::vector<RockHeadTrap> rockHeads;
    std::vector<PendulumTrap> pendulums;
    std::array<MudParticle, MAX_MUD_PARTICLES> mudParticles{};
    std::array<FlyingEnemy, MAX_FLYING_ENEMIES> flyingEnemies{};
    std::array<TridentProjectile, MAX_TRIDENT_PROJECTILES> projectiles{};
    uint8_t flyingEnemyCount = 0;
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
void configureRockHeadTravelBounds(const GameMap& map, std::vector<RockHeadTrap>& rocks);
void updateRockHeads(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt);
void updatePendulums(const LevelRuntime& runtime, WorldState& world, float timeSec);
void updateSawTraps(const LevelRuntime& runtime, WorldState& world, float timeSec);
void alignSawTrapsToMap(const GameMap& map, std::vector<SawTrap>& saws);
void configureSawTravelBounds(const GameMap& map, std::vector<SawTrap>& saws);
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

void initFlyingEnemiesForLevel(LevelRuntime& runtime, uint8_t globalLevelIndex, const std::string& tmxPath,
                               int tmxTileWidth = 16);
void updateFlyingEnemiesAndProjectiles(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt);
bool sampleFlyingEnemyHazard(const LevelRuntime& runtime, const PlayerState& player);
bool sampleProjectileHazard(const LevelRuntime& runtime, const PlayerState& player);

std::vector<PowerUpSpawn> loadPowerUpSpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth = 16);
void updatePowerUpDrops(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt);
void removeUncollectedLandedPowerUps(LevelRuntime& runtime);
void collectMagnetDrops(PlayerState& player, LevelRuntime& runtime);
void updateMagnetPulls(LevelRuntime& runtime, GameMap& map, WorldState& world, const std::vector<Pickup>& pickups,
                       float dt);
void syncMagnetState(const LevelRuntime& runtime, WorldState& world);

}  // namespace fireice
