#include "LevelMechanics.hpp"

#include "Paths.hpp"
#include "Physics.hpp"
#include "Pickup.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
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

float propertyFloat(const std::string& tag, const char* name, float fallback) {
    const std::string propNeedle = "<property";
    std::size_t pos = 0;
    while (true) {
        pos = tag.find(propNeedle, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t propEnd = tag.find("/>", pos);
        if (propEnd == std::string::npos) {
            break;
        }
        const std::string propTag = tag.substr(pos, propEnd - pos + 2);
        pos = propEnd + 2;
        const std::string nameKey = std::string("name=\"") + name + "\"";
        if (propTag.find(nameKey) == std::string::npos) {
            continue;
        }
        const std::string valueKey = "value=\"";
        const std::size_t valuePos = propTag.find(valueKey);
        if (valuePos == std::string::npos) {
            return fallback;
        }
        const std::size_t valueStart = valuePos + valueKey.size();
        const std::size_t valueEnd = propTag.find('"', valueStart);
        if (valueEnd == std::string::npos) {
            return fallback;
        }
        return std::stof(propTag.substr(valueStart, valueEnd - valueStart));
    }
    return fallback;
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
    for (RockHeadTrap& rock : runtime.rockHeads) {
        rock.x = rock.baseX;
        rock.y = rock.baseY;
        rock.dirX = rock.startDirX;
        rock.dirY = rock.startDirY;
        rock.waitTimer = 0.0f;
    }
    runtime.flyingEnemyCount = 0;
    for (FlyingEnemy& enemy : runtime.flyingEnemies) {
        enemy = FlyingEnemy{};
    }
    for (TridentProjectile& projectile : runtime.projectiles) {
        projectile = TridentProjectile{};
    }
    for (MagnetDrop& drop : runtime.magnetDrops) {
        drop = MagnetDrop{};
    }
    for (MagnetPull& pull : runtime.magnetPulls) {
        pull = MagnetPull{};
    }
    runtime.magnetSpawnsRemaining = 0;
    runtime.magnetSpawnTimer = 0.0f;
    runtime.powerUpLevelTimer = 0.0f;
    runtime.nextPowerUpScheduleIndex = 0;
    runtime.nextMagnetSpawnIndex = 0;
    runtime.nextSpeedSpawnIndex = 0;
    runtime.magnetSpawnInitialized = false;
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

namespace {

void updateMagnetSpawnsAndDrops(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt);

}  // namespace

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

namespace {

bool tagHasName(const std::string& tag, const char* name) {
    const std::string key = std::string("name=\"") + name + "\"";
    return tag.find(key) != std::string::npos;
}

bool isSolidForMagnetDrop(const GameMap& map, float x, float y) {
    const int tx = static_cast<int>(std::floor(x / TILE_SIZE));
    const int ty = static_cast<int>(std::floor(y / TILE_SIZE));
    if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) {
        return false;
    }
    const TileType type = map.tileAt(tx, ty);
    return type == TileType::Solid || type == TileType::OneWayPlatform;
}

MagnetPull* findMagnetPull(LevelRuntime& runtime, bool isGem, int gemTx, int gemTy, uint8_t pickupIndex) {
    for (MagnetPull& pull : runtime.magnetPulls) {
        if (!pull.active) {
            continue;
        }
        if (isGem && pull.isGem && pull.gemTx == gemTx && pull.gemTy == gemTy) {
            return &pull;
        }
        if (!isGem && !pull.isGem && pull.pickupIndex == pickupIndex) {
            return &pull;
        }
    }
    return nullptr;
}

MagnetPull* allocMagnetPull(LevelRuntime& runtime) {
    for (MagnetPull& pull : runtime.magnetPulls) {
        if (!pull.active) {
            return &pull;
        }
    }
    return nullptr;
}

bool isPickupCollected(uint8_t pickupIndex, uint32_t collectedMask, uint32_t collectedMaskHi,
                       uint32_t collectedMaskExt) {
    const uint32_t bit = 1u << (pickupIndex % 32u);
    const uint8_t word = pickupIndex / 32u;
    if (word > 2) {
        return true;
    }
    const uint32_t maskWord = word == 0 ? collectedMask : (word == 1 ? collectedMaskHi : collectedMaskExt);
    return (maskWord & bit) != 0;
}

void collectGemPull(MagnetPull& pull, GameMap& map, PlayerState& player) {
    if (map.tileAt(pull.gemTx, pull.gemTy) == TileType::Gem) {
        map.setTile(pull.gemTx, pull.gemTy, TileType::Empty);
        ++player.gems;
    }
    pull.active = false;
}

void collectFruitPull(MagnetPull& pull, PlayerState& player, uint32_t& collectedMask, uint32_t& collectedMaskHi,
                      uint32_t& collectedMaskExt) {
    const uint32_t bit = 1u << (pull.pickupIndex % 32u);
    const uint8_t word = pull.pickupIndex / 32u;
    if (word > 2 || isPickupCollected(pull.pickupIndex, collectedMask, collectedMaskHi, collectedMaskExt)) {
        pull.active = false;
        return;
    }
    if (word == 0) {
        collectedMask |= bit;
    } else if (word == 1) {
        collectedMaskHi |= bit;
    } else {
        collectedMaskExt |= bit;
    }
    ++player.gems;
    pull.active = false;
}

const PowerUpSpawn* pickPowerUpSpawn(const LevelRuntime& runtime, PowerUpKind kind, int& cursor) {
    std::vector<const PowerUpSpawn*> matches;
    for (const PowerUpSpawn& spawn : runtime.powerUpSpawns) {
        if (spawn.kind == kind) {
            matches.push_back(&spawn);
        }
    }
    if (matches.empty()) {
        for (const PowerUpSpawn& spawn : runtime.powerUpSpawns) {
            matches.push_back(&spawn);
        }
    }
    if (matches.empty()) {
        return nullptr;
    }
    const PowerUpSpawn* picked = matches[static_cast<std::size_t>(cursor % static_cast<int>(matches.size()))];
    cursor = (cursor + 1) % static_cast<int>(matches.size());
    return picked;
}

void applyPowerUpEffect(PlayerState& player, PowerUpKind kind) {
    if (kind == PowerUpKind::SpeedBoost) {
        player.speedBoostTimer =
            std::min(MAX_POWERUP_TIMER, std::max(0.0f, player.speedBoostTimer) + SPEED_BOOST_DURATION);
    } else {
        player.magnetTimer = std::min(MAX_POWERUP_TIMER, std::max(0.0f, player.magnetTimer) + MAGNET_DURATION);
    }
}

bool trySpawnPowerUpDrop(LevelRuntime& runtime, PowerUpKind kind, int& spawnCursor) {
    const PowerUpSpawn* spawnPoint = pickPowerUpSpawn(runtime, kind, spawnCursor);
    if (spawnPoint == nullptr) {
        return false;
    }

    for (MagnetDrop& drop : runtime.magnetDrops) {
        if (drop.active) {
            continue;
        }
        drop = MagnetDrop{};
        drop.active = true;
        drop.kind = static_cast<uint8_t>(kind);
        drop.x = spawnPoint->x;
        drop.y = POWERUP_SPAWN_Y;
        drop.vy = 0.0f;
        drop.falling = true;
        return true;
    }
    return false;
}

bool isSolidForPowerUpLanding(const GameMap& map, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) {
        return false;
    }
    return map.tileAt(tx, ty) == TileType::Solid;
}

int findLowestSolidRow(const GameMap& map, float x) {
    int bestRow = -1;
    const float probes[] = {x - 10.0f, x, x + 10.0f};
    for (const float probeX : probes) {
        const int tx = static_cast<int>(std::floor(probeX / TILE_SIZE));
        if (tx < 0 || tx >= map.width()) {
            continue;
        }
        for (int ty = map.height() - 1; ty >= 0; --ty) {
            if (map.tileAt(tx, ty) == TileType::Solid) {
                if (ty > bestRow) {
                    bestRow = ty;
                }
                break;
            }
        }
    }
    return bestRow;
}

bool advancePowerUpDropFall(const GameMap& map, MagnetDrop& drop, float dt) {
    if (!drop.falling) {
        return false;
    }

    drop.vy = std::min(drop.vy + POWERUP_FALL_GRAVITY * dt, POWERUP_MAX_FALL_SPEED);
    const float nextY = drop.y + drop.vy * dt;
    const float nextFeet = nextY + POWERUP_DROP_HEIGHT;
    const float mapBottom = static_cast<float>(map.height()) * TILE_SIZE;

    const int landRow = findLowestSolidRow(map, drop.x);
    if (landRow < 0) {
        drop.y = nextY;
        if (nextFeet >= mapBottom) {
            drop.falling = false;
            drop.landedTimer = POWERUP_LANDED_LIFETIME;
            return true;
        }
        return false;
    }

    const float landFeetY = static_cast<float>(landRow) * TILE_SIZE;
    if (nextFeet >= landFeetY - 0.5f) {
        drop.y = landFeetY - POWERUP_DROP_HEIGHT;
        drop.vy = 0.0f;
        drop.falling = false;
        drop.landedTimer = POWERUP_LANDED_LIFETIME;
        return true;
    }

    drop.y = nextY;
    return false;
}

void updateMagnetSpawnsAndDrops(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt) {
    if (world.phase != GamePhase::Playing || runtime.powerUpSpawns.empty()) {
        return;
    }

    if (!runtime.magnetSpawnInitialized) {
        runtime.magnetSpawnInitialized = true;
        runtime.powerUpLevelTimer = 0.0f;
        runtime.nextPowerUpScheduleIndex = 0;
    }

    runtime.powerUpLevelTimer += dt;

    struct ScheduledPowerUpWave {
        float atTime = 0.0f;
    };
    static constexpr ScheduledPowerUpWave kWaveSchedule[] = {
        {6.0f},
        {24.0f},
        {42.0f},
    };

    while (runtime.nextPowerUpScheduleIndex < sizeof(kWaveSchedule) / sizeof(kWaveSchedule[0]) &&
           runtime.powerUpLevelTimer >= kWaveSchedule[runtime.nextPowerUpScheduleIndex].atTime) {
        ++runtime.nextPowerUpScheduleIndex;
        trySpawnPowerUpDrop(runtime, PowerUpKind::Magnet, runtime.nextMagnetSpawnIndex);
        trySpawnPowerUpDrop(runtime, PowerUpKind::SpeedBoost, runtime.nextSpeedSpawnIndex);
    }

    for (MagnetDrop& drop : runtime.magnetDrops) {
        if (!drop.active) {
            continue;
        }
        if (drop.falling) {
            advancePowerUpDropFall(map, drop, dt);
        } else if (drop.landedTimer > 0.0f) {
            drop.landedTimer = std::max(0.0f, drop.landedTimer - dt);
        }
    }
}

}  // namespace

void updatePowerUpDrops(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt) {
    updateMagnetSpawnsAndDrops(runtime, map, world, dt);
}

void removeUncollectedLandedPowerUps(LevelRuntime& runtime) {
    for (MagnetDrop& drop : runtime.magnetDrops) {
        if (drop.active && !drop.falling && drop.landedTimer <= 0.0f) {
            drop.active = false;
        }
    }
}

void syncMagnetState(const LevelRuntime& runtime, WorldState& world) {
    world.magnetSpawnsRemaining = runtime.magnetSpawnsRemaining;
    world.magnetDropCount = 0;
    for (const MagnetDrop& drop : runtime.magnetDrops) {
        if (!drop.active || world.magnetDropCount >= MAX_MAGNET_DROPS) {
            continue;
        }
        world.magnetDrops[world.magnetDropCount].x = drop.x;
        world.magnetDrops[world.magnetDropCount].y = drop.y;
        world.magnetDrops[world.magnetDropCount].kind = drop.kind;
        world.magnetDrops[world.magnetDropCount].falling = drop.falling ? 1 : 0;
        world.magnetDrops[world.magnetDropCount].active = 1;
        ++world.magnetDropCount;
    }

    world.magnetPullCount = 0;
    for (const MagnetPull& pull : runtime.magnetPulls) {
        if (!pull.active || world.magnetPullCount >= MAX_MAGNET_PULLS) {
            continue;
        }
        WorldState::SyncMagnetPull& sync = world.magnetPulls[world.magnetPullCount];
        sync.x = pull.x;
        sync.y = pull.y;
        sync.kind = pull.isGem ? 0 : 1;
        sync.pickupIndex = pull.pickupIndex;
        sync.gemTx = static_cast<int16_t>(pull.gemTx);
        sync.gemTy = static_cast<int16_t>(pull.gemTy);
        sync.active = 1;
        ++world.magnetPullCount;
    }
}

void collectMagnetDrops(PlayerState& player, LevelRuntime& runtime) {
    if (!player.alive) {
        return;
    }

    const AABB playerBox = playerBounds(player);
    for (MagnetDrop& drop : runtime.magnetDrops) {
        if (!drop.active) {
            continue;
        }
        const AABB dropBox{drop.x - POWERUP_DROP_WIDTH * 0.5f, drop.y, POWERUP_DROP_WIDTH, POWERUP_DROP_HEIGHT};
        if (!playerBox.intersects(dropBox)) {
            continue;
        }
        drop.active = false;
        applyPowerUpEffect(player, static_cast<PowerUpKind>(drop.kind));
    }
}

void updateMagnetPulls(LevelRuntime& runtime, GameMap& map, WorldState& world, const std::vector<Pickup>& pickups,
                       float dt) {
    for (PlayerState& player : world.players) {
        if (!player.alive) {
            continue;
        }
        if (player.magnetTimer > 0.0f) {
            player.magnetTimer = std::max(0.0f, player.magnetTimer - dt);
        }
        if (player.speedBoostTimer > 0.0f) {
            player.speedBoostTimer = std::max(0.0f, player.speedBoostTimer - dt);
        }
    }

    for (PlayerState& player : world.players) {
        if (!player.alive || player.magnetTimer <= 0.0f) {
            continue;
        }

        const float cx = player.x + PLAYER_WIDTH * 0.5f;
        const float cy = player.y + PLAYER_HEIGHT * 0.5f;
        const float radiusSq = MAGNET_RADIUS * MAGNET_RADIUS;

        const int minTx = static_cast<int>(std::floor((cx - MAGNET_RADIUS) / TILE_SIZE));
        const int maxTx = static_cast<int>(std::floor((cx + MAGNET_RADIUS) / TILE_SIZE));
        const int minTy = static_cast<int>(std::floor((cy - MAGNET_RADIUS) / TILE_SIZE));
        const int maxTy = static_cast<int>(std::floor((cy + MAGNET_RADIUS) / TILE_SIZE));

        for (int ty = minTy; ty <= maxTy; ++ty) {
            for (int tx = minTx; tx <= maxTx; ++tx) {
                if (map.tileAt(tx, ty) != TileType::Gem) {
                    continue;
                }
                if (findMagnetPull(runtime, true, tx, ty, 0) != nullptr) {
                    continue;
                }
                const float gemCx = (static_cast<float>(tx) + 0.5f) * TILE_SIZE;
                const float gemCy = (static_cast<float>(ty) + 0.5f) * TILE_SIZE;
                const float dx = gemCx - cx;
                const float dy = gemCy - cy;
                if (dx * dx + dy * dy > radiusSq) {
                    continue;
                }
                MagnetPull* pull = allocMagnetPull(runtime);
                if (pull == nullptr) {
                    continue;
                }
                pull->active = true;
                pull->isGem = true;
                pull->gemTx = tx;
                pull->gemTy = ty;
                pull->originX = gemCx;
                pull->originY = gemCy;
                pull->x = gemCx;
                pull->y = gemCy;
            }
        }

        for (const Pickup& pickup : pickups) {
            if (pickup.kind != PickupKind::Fruit) {
                continue;
            }
            if (isPickupCollected(pickup.index, world.collectedPickupsMask, world.collectedPickupsMaskHi,
                                  world.collectedPickupsMaskExt)) {
                continue;
            }
            if (findMagnetPull(runtime, false, 0, 0, pickup.index) != nullptr) {
                continue;
            }
            const float pickupCx = pickup.x + pickup.w * 0.5f;
            const float pickupCy = pickup.y + pickup.h * 0.5f;
            const float dx = pickupCx - cx;
            const float dy = pickupCy - cy;
            if (dx * dx + dy * dy > radiusSq) {
                continue;
            }
            MagnetPull* pull = allocMagnetPull(runtime);
            if (pull == nullptr) {
                continue;
            }
            pull->active = true;
            pull->isGem = false;
            pull->pickupIndex = pickup.index;
            pull->originX = pickupCx;
            pull->originY = pickupCy;
            pull->x = pickupCx;
            pull->y = pickupCy;
        }
    }

    for (MagnetPull& pull : runtime.magnetPulls) {
        if (!pull.active) {
            continue;
        }

        PlayerState* owner = nullptr;
        float bestDistSq = std::numeric_limits<float>::max();
        for (PlayerState& player : world.players) {
            if (!player.alive || player.magnetTimer <= 0.0f) {
                continue;
            }
            const float cx = player.x + PLAYER_WIDTH * 0.5f;
            const float cy = player.y + PLAYER_HEIGHT * 0.5f;
            const float dx = pull.x - cx;
            const float dy = pull.y - cy;
            const float distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                owner = &player;
            }
        }
        if (owner == nullptr) {
            pull.active = false;
            continue;
        }

        const float targetX = owner->x + PLAYER_WIDTH * 0.5f;
        const float targetY = owner->y + PLAYER_HEIGHT * 0.5f;
        const float dx = targetX - pull.x;
        const float dy = targetY - pull.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= MAGNET_COLLECT_DISTANCE) {
            if (pull.isGem) {
                collectGemPull(pull, map, *owner);
            } else {
                collectFruitPull(pull, *owner, world.collectedPickupsMask, world.collectedPickupsMaskHi,
                                 world.collectedPickupsMaskExt);
            }
            continue;
        }

        const float speed =
            MAGNET_PULL_SPEED * (0.55f + 0.45f * (1.0f - std::min(dist, MAGNET_RADIUS) / MAGNET_RADIUS));
        const float step = speed * dt;
        if (dist <= step || dist <= 0.001f) {
            pull.x = targetX;
            pull.y = targetY;
        } else {
            pull.x += dx / dist * step;
            pull.y += dy / dist * step;
        }
    }
}

std::vector<PowerUpSpawn> loadPowerUpSpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    std::vector<PowerUpSpawn> spawns;
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
        PowerUpKind kind = PowerUpKind::Magnet;
        if (tagHasName(tag, "speed_spawn")) {
            kind = PowerUpKind::SpeedBoost;
        } else if (!tagHasName(tag, "magnet_spawn")) {
            continue;
        }

        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float ow = attrFloat(tag, "width", static_cast<float>(tmxTileWidth));
        const float oh = attrFloat(tag, "height", static_cast<float>(tmxTileWidth));

        PowerUpSpawn spawn;
        spawn.x = (ox + ow * 0.5f) * scale;
        spawn.kind = kind;
        spawns.push_back(spawn);
    }

    return spawns;
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

float sawOffsetY(const SawTrap& saw, float timeSec) {
    return std::sin(timeSec * saw.speed + saw.phase) * (saw.travelRange * 0.5f);
}

float sawCurrentY(const SawTrap& saw, float timeSec) {
    return saw.anchorY + sawOffsetY(saw, timeSec);
}

AABB sawHitbox(const SawTrap& saw, float timeSec) {
    const float hitW = saw.width * SAW_HITBOX_SCALE;
    const float hitH = saw.height * SAW_HITBOX_SCALE;
    const float y = sawCurrentY(saw, timeSec);
    return {saw.anchorX + (saw.width - hitW) * 0.5f, y + (saw.height - hitH) * 0.5f, hitW, hitH};
}

bool sampleSawHazard(const std::vector<SawTrap>& saws, const PlayerState& player, float timeSec) {
    if (!player.alive || saws.empty()) {
        return false;
    }

    const AABB box = playerBounds(player);
    for (const SawTrap& saw : saws) {
        if (box.intersects(sawHitbox(saw, timeSec))) {
            return true;
        }
    }
    return false;
}

AABB rockHeadHitbox(const RockHeadTrap& rock) {
    const float hitW = rock.width * ROCK_HEAD_HITBOX_SCALE;
    const float hitH = rock.height * ROCK_HEAD_HITBOX_SCALE;
    return {rock.x + (rock.width - hitW) * 0.5f, rock.y + (rock.height - hitH) * 0.5f, hitW, hitH};
}

namespace {

constexpr float ROCK_HEAD_EDGE_INSET = 2.0f;
constexpr float ROCK_HEAD_SCAN_STEP = 1.0f;
constexpr float ROCK_HEAD_FLOOR_SKIP = TILE_SIZE * 0.5f;
constexpr float ROCK_HEAD_WALL_SKIP = TILE_SIZE * 0.5f;

bool rockColumnHasSolid(const GameMap& map, float x, float top, float bottom) {
    const int tx = static_cast<int>(std::floor(x / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(top / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((bottom - 0.01f) / TILE_SIZE));
    for (int ty = minY; ty <= maxY; ++ty) {
        if (map.isSolid(map.tileAt(tx, ty))) {
            return true;
        }
    }
    return false;
}

bool rockRowHasSolid(const GameMap& map, float left, float right, float y) {
    const int ty = static_cast<int>(std::floor(y / TILE_SIZE));
    const int minX = static_cast<int>(std::floor(left / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((right - 0.01f) / TILE_SIZE));
    for (int tx = minX; tx <= maxX; ++tx) {
        if (map.isSolid(map.tileAt(tx, ty))) {
            return true;
        }
    }
    return false;
}

bool rockMovementBlocked(const GameMap& map, const RockHeadTrap& rock, float nextX, float nextY, int dirX, int dirY) {
    const float inset = ROCK_HEAD_EDGE_INSET;
    const float top = nextY + inset;
    const float bottom = nextY + rock.height - inset;

    if (dirX > 0) {
        const float sideBottom = std::max(top + 1.0f, bottom - ROCK_HEAD_FLOOR_SKIP);
        return rockColumnHasSolid(map, nextX + rock.width - inset, top, sideBottom);
    }
    if (dirX < 0) {
        const float sideBottom = std::max(top + 1.0f, bottom - ROCK_HEAD_FLOOR_SKIP);
        return rockColumnHasSolid(map, nextX + inset, top, sideBottom);
    }
    if (dirY > 0) {
        float left = nextX + inset + ROCK_HEAD_WALL_SKIP;
        float right = nextX + rock.width - inset - ROCK_HEAD_WALL_SKIP;
        if (right <= left) {
            left = nextX + inset;
            right = nextX + rock.width - inset;
        }
        return rockRowHasSolid(map, left, right, nextY + rock.height - inset);
    }
    if (dirY < 0) {
        float left = nextX + inset + ROCK_HEAD_WALL_SKIP;
        float right = nextX + rock.width - inset - ROCK_HEAD_WALL_SKIP;
        if (right <= left) {
            left = nextX + inset;
            right = nextX + rock.width - inset;
        }
        return rockRowHasSolid(map, left, right, nextY + inset);
    }
    return false;
}

float scanRockHorizontalBound(const GameMap& map, const RockHeadTrap& rock, int dirX) {
    float x = rock.baseX;
    if (dirX < 0) {
        while (x > ROCK_HEAD_SCAN_STEP && !rockMovementBlocked(map, rock, x - ROCK_HEAD_SCAN_STEP, rock.baseY, -1, 0)) {
            x -= ROCK_HEAD_SCAN_STEP;
        }
        return x;
    }

    const float mapRight = static_cast<float>(map.width()) * TILE_SIZE;
    while (x + rock.width < mapRight - ROCK_HEAD_SCAN_STEP &&
           !rockMovementBlocked(map, rock, x + ROCK_HEAD_SCAN_STEP, rock.baseY, 1, 0)) {
        x += ROCK_HEAD_SCAN_STEP;
    }
    return x;
}

float scanRockVerticalBound(const GameMap& map, const RockHeadTrap& rock, int dirY) {
    float y = rock.baseY;
    if (dirY < 0) {
        while (y > ROCK_HEAD_SCAN_STEP && !rockMovementBlocked(map, rock, rock.baseX, y - ROCK_HEAD_SCAN_STEP, 0, -1)) {
            y -= ROCK_HEAD_SCAN_STEP;
        }
        return y;
    }

    const float mapBottom = static_cast<float>(map.height()) * TILE_SIZE;
    while (y + rock.height < mapBottom - ROCK_HEAD_SCAN_STEP &&
           !rockMovementBlocked(map, rock, rock.baseX, y + ROCK_HEAD_SCAN_STEP, 0, 1)) {
        y += ROCK_HEAD_SCAN_STEP;
    }
    return y;
}

bool playerOverlapsSolid(const PlayerState& player, const GameMap& map) {
    const AABB box = playerBounds(player);
    const int minX = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((box.right() - 0.01f) / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((box.bottom() - 0.01f) / TILE_SIZE));
    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (map.isSolid(map.tileAt(tx, ty))) {
                return true;
            }
        }
    }
    return false;
}

void pushPlayerByRock(PlayerState& player, const RockHeadTrap& rock, const AABB& previousBox, const GameMap& map,
                      float dx, float dy) {
    if (!player.alive) {
        return;
    }
    const AABB rockBox = rockHeadHitbox(rock);
    AABB playerBox = playerBounds(player);
    if (!playerBox.intersects(rockBox)) {
        return;
    }

    const bool horizontal = std::abs(dx) >= std::abs(dy);
    if (horizontal) {
        if (dx > 0.0f) {
            player.x = rockBox.right();
        } else if (dx < 0.0f) {
            player.x = rockBox.left() - PLAYER_WIDTH;
        } else if (playerBox.x < previousBox.x) {
            player.x = rockBox.left() - PLAYER_WIDTH;
        } else {
            player.x = rockBox.right();
        }
        player.vx = dx > 0.0f ? ROCK_HEAD_SPEED : -ROCK_HEAD_SPEED;
    } else {
        if (dy > 0.0f) {
            player.y = rockBox.bottom();
            player.vy = ROCK_HEAD_SPEED;
        } else if (dy < 0.0f) {
            player.y = rockBox.top() - PLAYER_HEIGHT;
            player.vy = -ROCK_HEAD_SPEED;
            player.onGround = true;
        }
    }

    if (playerOverlapsSolid(player, map)) {
        player.alive = false;
    }
}

void syncRockHeadsToWorld(const LevelRuntime& runtime, WorldState& world) {
    world.rockHeadCount =
        static_cast<uint8_t>(std::min(runtime.rockHeads.size(), static_cast<std::size_t>(MAX_ROCK_HEAD_TRAPS)));
    for (uint8_t i = 0; i < world.rockHeadCount; ++i) {
        const RockHeadTrap& rock = runtime.rockHeads[i];
        world.rockHeads[i].x = rock.x;
        world.rockHeads[i].y = rock.y;
        world.rockHeads[i].w = rock.width;
        world.rockHeads[i].h = rock.height;
        world.rockHeads[i].gid = rock.gid;
        world.rockHeads[i].dirX = static_cast<int8_t>(rock.dirX);
        world.rockHeads[i].dirY = static_cast<int8_t>(rock.dirY);
        world.rockHeads[i].active = 1;
    }
    for (uint8_t i = world.rockHeadCount; i < MAX_ROCK_HEAD_TRAPS; ++i) {
        world.rockHeads[i] = WorldState::SyncRockHead{};
    }
}

void configureRockHeadTravelBoundsImpl(const GameMap& map, std::vector<RockHeadTrap>& rocks) {
    for (RockHeadTrap& rock : rocks) {
        if (rock.dirY != 0) {
            rock.minX = rock.maxX = rock.baseX;
            rock.minY = scanRockVerticalBound(map, rock, -1);
            rock.maxY = scanRockVerticalBound(map, rock, 1);
        } else {
            rock.minY = rock.maxY = rock.baseY;
            rock.minX = scanRockHorizontalBound(map, rock, -1);
            rock.maxX = scanRockHorizontalBound(map, rock, 1);
        }
    }
}

}  // namespace

void configureRockHeadTravelBounds(const GameMap& map, std::vector<RockHeadTrap>& rocks) {
    configureRockHeadTravelBoundsImpl(map, rocks);
}

void updateRockHeads(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt) {
    for (RockHeadTrap& rock : runtime.rockHeads) {
        const AABB previousBox = rockHeadHitbox(rock);
        if (rock.waitTimer > 0.0f) {
            rock.waitTimer = std::max(0.0f, rock.waitTimer - dt);
            continue;
        }

        const float dx = static_cast<float>(rock.dirX) * ROCK_HEAD_SPEED * dt;
        const float dy = static_cast<float>(rock.dirY) * ROCK_HEAD_SPEED * dt;
        float nextX = rock.x + dx;
        float nextY = rock.y + dy;
        bool hit = false;

        if (rock.dirX != 0) {
            if (rockMovementBlocked(map, rock, nextX, rock.y, rock.dirX, 0)) {
                hit = true;
                nextX = rock.x;
            } else if ((rock.dirX < 0 && nextX <= rock.minX) || (rock.dirX > 0 && nextX >= rock.maxX)) {
                hit = true;
                nextX = std::clamp(nextX, rock.minX, rock.maxX);
            }
        } else if (rock.dirY != 0) {
            if (rockMovementBlocked(map, rock, rock.x, nextY, 0, rock.dirY)) {
                hit = true;
                nextY = rock.y;
            } else if ((rock.dirY < 0 && nextY <= rock.minY) || (rock.dirY > 0 && nextY >= rock.maxY)) {
                hit = true;
                nextY = std::clamp(nextY, rock.minY, rock.maxY);
            }
        }

        rock.x = nextX;
        rock.y = nextY;
        for (PlayerState& player : world.players) {
            pushPlayerByRock(player, rock, previousBox, map, dx, dy);
        }

        if (hit) {
            rock.waitTimer = ROCK_HEAD_WAIT_TIME;
            rock.dirX = -rock.dirX;
            rock.dirY = -rock.dirY;
        }
    }
    syncRockHeadsToWorld(runtime, world);
}

namespace {

Vec2 pendulumBallCenter(const PendulumTrap& pendulum, float timeSec) {
    const float angle = std::sin(timeSec * PENDULUM_SPEED + pendulum.phase) * PENDULUM_MAX_ANGLE;
    return {pendulum.pivotX + std::sin(angle) * pendulum.chainLength,
            pendulum.pivotY + std::cos(angle) * pendulum.chainLength};
}

AABB pendulumBallHitbox(const PendulumTrap& pendulum, float timeSec) {
    const Vec2 center = pendulumBallCenter(pendulum, timeSec);
    const float hitW = pendulum.ballW * PENDULUM_BALL_HITBOX_SCALE;
    const float hitH = pendulum.ballH * PENDULUM_BALL_HITBOX_SCALE;
    return {center.x - hitW * 0.5f, center.y - hitH * 0.5f, hitW, hitH};
}

}  // namespace

bool samplePendulumHazard(const std::vector<PendulumTrap>& pendulums, const PlayerState& player, float timeSec) {
    if (!player.alive || pendulums.empty()) {
        return false;
    }
    const AABB box = playerBounds(player);
    for (const PendulumTrap& pendulum : pendulums) {
        if (box.intersects(pendulumBallHitbox(pendulum, timeSec))) {
            return true;
        }
    }
    return false;
}

void updatePendulums(const LevelRuntime& runtime, WorldState& world, float timeSec) {
    world.pendulumCount =
        static_cast<uint8_t>(std::min(runtime.pendulums.size(), static_cast<std::size_t>(MAX_PENDULUM_TRAPS)));
    for (uint8_t i = 0; i < world.pendulumCount; ++i) {
        const PendulumTrap& pendulum = runtime.pendulums[i];
        const Vec2 center = pendulumBallCenter(pendulum, timeSec);
        world.pendulums[i].pivotX = pendulum.pivotX;
        world.pendulums[i].pivotY = pendulum.pivotY;
        world.pendulums[i].ballX = center.x - pendulum.ballW * 0.5f;
        world.pendulums[i].ballY = center.y - pendulum.ballH * 0.5f;
        world.pendulums[i].ballW = pendulum.ballW;
        world.pendulums[i].ballH = pendulum.ballH;
        world.pendulums[i].ballGid = pendulum.ballGid;
        world.pendulums[i].chainGid = pendulum.chainGid;
        world.pendulums[i].chainCount = pendulum.chainCount;
        world.pendulums[i].active = 1;
    }
    for (uint8_t i = world.pendulumCount; i < MAX_PENDULUM_TRAPS; ++i) {
        world.pendulums[i] = WorldState::SyncPendulum{};
    }
}

void updateSawTraps(const LevelRuntime& runtime, WorldState& world, float timeSec) {
    world.sawCount = static_cast<uint8_t>(std::min(runtime.sawTraps.size(), static_cast<std::size_t>(MAX_SAW_TRAPS)));
    for (uint8_t i = 0; i < world.sawCount; ++i) {
        const SawTrap& saw = runtime.sawTraps[i];
        world.saws[i].x = saw.anchorX;
        world.saws[i].y = sawCurrentY(saw, timeSec);
        world.saws[i].w = saw.width;
        world.saws[i].h = saw.height;
        world.saws[i].gid = saw.gid;
        world.saws[i].active = 1;
    }
    for (uint8_t i = world.sawCount; i < MAX_SAW_TRAPS; ++i) {
        world.saws[i] = WorldState::SyncSaw{};
    }
}

namespace {

float solidRowBottomY(const GameMap& map, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) {
        return 0.0f;
    }
    if (!map.isSolid(map.tileAt(tx, ty))) {
        return 0.0f;
    }
    return static_cast<float>((ty + 1) * TILE_SIZE);
}

float solidRowTopY(const GameMap& map, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) {
        return static_cast<float>(map.height() * TILE_SIZE);
    }
    if (!map.isSolid(map.tileAt(tx, ty))) {
        return static_cast<float>(map.height() * TILE_SIZE);
    }
    return static_cast<float>(ty * TILE_SIZE);
}

}  // namespace

void alignSawTrapsToMap(const GameMap& map, std::vector<SawTrap>& saws) {
    if (map.width() <= 0 || map.height() <= 0) {
        return;
    }

    for (SawTrap& saw : saws) {
        const float centerX = saw.anchorX + saw.width * 0.5f;
        int tx = static_cast<int>(centerX / TILE_SIZE);
        tx = std::clamp(tx, 0, map.width() - 1);

        const int anchorRow = std::clamp(static_cast<int>(saw.anchorY / TILE_SIZE), 0, map.height() - 1);

        float ceilingBottom = 0.0f;
        for (int ty = anchorRow - 1; ty >= 0; --ty) {
            const float bottom = solidRowBottomY(map, tx, ty);
            if (bottom > 0.0f) {
                ceilingBottom = bottom;
                break;
            }
        }

        float floorTop = static_cast<float>(map.height() * TILE_SIZE);
        for (int ty = anchorRow + 1; ty < map.height(); ++ty) {
            const float top = solidRowTopY(map, tx, ty);
            if (top < static_cast<float>(map.height() * TILE_SIZE)) {
                floorTop = top;
                break;
            }
        }

        if (floorTop - ceilingBottom > saw.height + TILE_SIZE * 0.5f) {
            saw.anchorY = ceilingBottom + (floorTop - ceilingBottom - saw.height) * 0.5f;
        }

        saw.anchorX = static_cast<float>(tx) * TILE_SIZE + (TILE_SIZE - saw.width) * 0.5f;
    }
}

void configureSawTravelBounds(const GameMap& map, std::vector<SawTrap>& saws) {
    constexpr float kTravelMargin = 6.0f;

    for (SawTrap& saw : saws) {
        const float centerX = saw.anchorX + saw.width * 0.5f;
        int tx = static_cast<int>(centerX / TILE_SIZE);
        tx = std::clamp(tx, 0, map.width() - 1);

        const int anchorRow = std::clamp(static_cast<int>(saw.anchorY / TILE_SIZE), 0, map.height() - 1);

        float ceilingBottom = 0.0f;
        for (int ty = anchorRow - 1; ty >= 0; --ty) {
            const float bottom = solidRowBottomY(map, tx, ty);
            if (bottom > 0.0f) {
                ceilingBottom = bottom;
                break;
            }
        }

        float floorTop = static_cast<float>(map.height() * TILE_SIZE);
        for (int ty = anchorRow + 1; ty < map.height(); ++ty) {
            const float top = solidRowTopY(map, tx, ty);
            if (top < static_cast<float>(map.height() * TILE_SIZE)) {
                floorTop = top;
                break;
            }
        }

        const float maxUpAmplitude =
            std::max(0.0f, saw.anchorY - (ceilingBottom + kTravelMargin));
        const float maxDownAmplitude =
            std::max(0.0f, (floorTop - kTravelMargin) - (saw.anchorY + saw.height));
        const float maxAmplitude = std::min(maxUpAmplitude, maxDownAmplitude);
        if (maxAmplitude > 0.0f) {
            saw.travelRange = std::min(saw.travelRange, maxAmplitude * 2.0f);
        }
    }
}

std::vector<PendulumTrap> loadPendulumsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    struct ObjectInfo {
        int gid = 0;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        Vec2 center{};
    };

    std::vector<ObjectInfo> balls;
    std::vector<ObjectInfo> chains;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const float scale = TILE_SIZE / static_cast<float>(std::max(1, tmxTileWidth));
    const std::vector<TmxTilesetInfo> tilesets = loadTmxTilesetInfo(tmxPath);

    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        const int gid = attrInt(tag, "gid", 0);
        if (!isSpikedBallGid(gid, tilesets) && !isChainGid(gid, tilesets)) {
            continue;
        }
        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float ow = attrFloat(tag, "width", static_cast<float>(tmxTileWidth));
        const float oh = attrFloat(tag, "height", static_cast<float>(tmxTileWidth));
        ObjectInfo obj;
        obj.gid = gid;
        obj.x = ox * scale;
        obj.y = (oy - oh) * scale;
        obj.w = ow * scale;
        obj.h = oh * scale;
        obj.center = {obj.x + obj.w * 0.5f, obj.y + obj.h * 0.5f};
        if (isSpikedBallGid(gid, tilesets)) {
            balls.push_back(obj);
        } else {
            chains.push_back(obj);
        }
    }

    std::sort(balls.begin(), balls.end(),
              [](const ObjectInfo& a, const ObjectInfo& b) { return a.center.x < b.center.x; });
    std::vector<PendulumTrap> pendulums;
    for (const ObjectInfo& ball : balls) {
        if (pendulums.size() >= MAX_PENDULUM_TRAPS) {
            break;
        }
        std::vector<ObjectInfo> candidates;
        for (const ObjectInfo& chain : chains) {
            if (chain.center.y < ball.center.y && std::abs(chain.center.x - ball.center.x) <= TILE_SIZE * 3.0f &&
                std::abs(chain.center.y - ball.center.y) <= TILE_SIZE * 4.0f) {
                candidates.push_back(chain);
            }
        }
        if (candidates.empty()) {
            continue;
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const ObjectInfo& a, const ObjectInfo& b) { return a.center.y < b.center.y; });
        const ObjectInfo& topChain = candidates.front();
        PendulumTrap pendulum;
        pendulum.pivotX = topChain.center.x;
        pendulum.pivotY = topChain.center.y - TILE_SIZE * 1.5f;
        pendulum.chainLength =
            std::max(TILE_SIZE, std::hypot(ball.center.x - pendulum.pivotX, ball.center.y - pendulum.pivotY));
        pendulum.ballW = ball.w;
        pendulum.ballH = ball.h;
        pendulum.ballGid = ball.gid;
        pendulum.chainGid = topChain.gid;
        pendulum.chainCount = static_cast<uint8_t>(std::clamp(static_cast<int>(candidates.size()), 1, 3));
        pendulum.phase = static_cast<float>(pendulums.size()) * 0.45f;
        pendulums.push_back(pendulum);
    }
    return pendulums;
}
std::vector<RockHeadTrap> loadRockHeadsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    std::vector<RockHeadTrap> rocks;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return rocks;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const float scale = TILE_SIZE / static_cast<float>(std::max(1, tmxTileWidth));
    const std::vector<TmxTilesetInfo> tilesets = loadTmxTilesetInfo(tmxPath);

    uint8_t index = 0;
    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        const int gid = attrInt(tag, "gid", 0);
        if (!isRockHeadGid(gid, tilesets)) {
            continue;
        }
        if (index >= MAX_ROCK_HEAD_TRAPS) {
            break;
        }
        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float ow = attrFloat(tag, "width", 42.0f);
        const float oh = attrFloat(tag, "height", 42.0f);

        RockHeadTrap rock;
        rock.x = ox * scale;
        rock.y = (oy - oh) * scale;
        rock.width = ow * scale;
        rock.height = oh * scale;
        rock.baseX = rock.x;
        rock.baseY = rock.y;
        rock.gid = gid;
        rocks.push_back(rock);
        ++index;
    }

    if (!rocks.empty()) {
        const auto rightmost = std::max_element(
            rocks.begin(), rocks.end(), [](const RockHeadTrap& a, const RockHeadTrap& b) { return a.baseX < b.baseX; });
        for (RockHeadTrap& rock : rocks) {
            if (&rock == &(*rightmost)) {
                rock.dirX = 0;
                rock.dirY = -1;
                rock.startDirX = 0;
                rock.startDirY = rock.dirY;
            } else {
                rock.dirX = rock.baseX < rightmost->baseX ? 1 : -1;
                rock.dirY = 0;
                rock.startDirX = rock.dirX;
                rock.startDirY = 0;
            }
        }
    }
    return rocks;
}

std::vector<SawTrap> loadSawTrapsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
    std::vector<SawTrap> saws;
    const std::string path = resolveAssetPath(tmxPath);
    std::ifstream file(path);
    if (!file.is_open()) {
        return saws;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string xml = buffer.str();
    const float scale = TILE_SIZE / static_cast<float>(std::max(1, tmxTileWidth));
    const std::vector<TmxTilesetInfo> tilesets = loadTmxTilesetInfo(tmxPath);

    uint8_t index = 0;
    for (const std::string& tag : findObjectTagsInGroups(xml)) {
        const int gid = attrInt(tag, "gid", 0);
        if (!isSawGid(gid, tilesets)) {
            continue;
        }
        if (index >= MAX_SAW_TRAPS) {
            break;
        }

        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float ow = attrFloat(tag, "width", 38.0f);
        const float oh = attrFloat(tag, "height", 38.0f);
        const float travelTmx = propertyFloat(tag, "travel", SAW_DEFAULT_TRAVEL_TMX);

        SawTrap saw;
        saw.anchorX = ox * scale;
        saw.anchorY = (oy - oh) * scale;
        saw.width = ow * scale;
        saw.height = oh * scale;
        saw.travelRange = travelTmx * scale;
        saw.speed = propertyFloat(tag, "speed", SAW_DEFAULT_SPEED);
        saw.phase = static_cast<float>(index) * 2.0943951f;
        saw.gid = gid;
        saws.push_back(saw);
        ++index;
    }

    return saws;
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
            if (x > 0) {
                const int leftGid = gids[static_cast<std::size_t>(y * mapWidth + (x - 1))] & 0x1FFFFFFF;
                if (leftGid >= kFanFirstGid && leftGid <= kFanLastGid) {
                    continue;
                }
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
            const int brickRow = x < mapWidth / 2 ? 10 : 11;
            zone.targetFeetY = static_cast<float>(brickRow) * TILE_SIZE - PLAYER_HEIGHT;
            zone.emitterX = static_cast<float>(x + 1) * TILE_SIZE;
            zone.emitterY = static_cast<float>(y + 1) * TILE_SIZE;
            zones.push_back(zone);
        }
    }

    (void) tmxTileWidth;
    return zones;
}

std::vector<Vec2> loadFlyingEnemySpawnsFromTmx(const std::string& tmxPath, int tmxTileWidth) {
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
        if (!tagHasName(tag, "flying_enemy")) {
            continue;
        }
        if (spawns.size() >= MAX_FLYING_ENEMIES) {
            break;
        }
        const float ox = attrFloat(tag, "x", 0.0f);
        const float oy = attrFloat(tag, "y", 0.0f);
        const float oh = attrFloat(tag, "height", 32.0f);
        spawns.push_back({ox * scale, (oy - oh * 0.5f) * scale});
    }
    return spawns;
}

AABB flyingEnemyHitbox(const FlyingEnemy& enemy) {
    return {enemy.x - ENEMY_HITBOX_W * 0.5f, enemy.y - ENEMY_HITBOX_H * 0.5f, ENEMY_HITBOX_W, ENEMY_HITBOX_H};
}

AABB tridentHitbox(const TridentProjectile& projectile) {
    const float size = TRIDENT_HITBOX;
    return {projectile.x - size * 0.5f, projectile.y - size * 0.5f, size, size};
}

PlayerState* nearestAlivePlayer(WorldState& world, float x, float y) {
    PlayerState* nearest = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (PlayerState& player : world.players) {
        if (!player.alive || player.role == PlayerRole::None) {
            continue;
        }
        const float dx = player.x + PLAYER_WIDTH * 0.5f - x;
        const float dy = player.y + PLAYER_HEIGHT * 0.5f - y;
        const float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            nearest = &player;
        }
    }
    return nearest;
}

TridentProjectile* acquireProjectile(LevelRuntime& runtime) {
    for (TridentProjectile& projectile : runtime.projectiles) {
        if (!projectile.active) {
            return &projectile;
        }
    }
    return nullptr;
}

void shootTridentAtPlayer(FlyingEnemy& enemy, LevelRuntime& runtime, const PlayerState& target) {
    TridentProjectile* projectile = acquireProjectile(runtime);
    if (projectile == nullptr) {
        return;
    }

    const float fromX = enemy.x;
    const float fromY = enemy.y;
    const float toX = target.x + PLAYER_WIDTH * 0.5f;
    const float toY = target.y + PLAYER_HEIGHT * 0.5f;
    const float dx = toX - fromX;
    const float dy = toY - fromY;
    const float len = std::hypot(dx, dy);
    if (len < 1.0f) {
        return;
    }

    projectile->x = fromX;
    projectile->y = fromY;
    projectile->vx = dx / len * TRIDENT_SPEED;
    projectile->vy = dy / len * TRIDENT_SPEED;
    projectile->rotation = std::atan2(projectile->vy, projectile->vx) * 180.0f / 3.14159265f + 90.0f;
    projectile->active = true;
}

bool projectileHitsSolid(const GameMap& map, const TridentProjectile& projectile) {
    const AABB box = tridentHitbox(projectile);
    const int minTx = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxTx = static_cast<int>(std::floor(box.right() / TILE_SIZE));
    const int minTy = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxTy = static_cast<int>(std::floor(box.bottom() / TILE_SIZE));
    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            const TileType tile = map.tileAt(tx, ty);
            if (tile == TileType::Solid || tile == TileType::OneWayPlatform) {
                return true;
            }
        }
    }
    return false;
}

void syncFlyingEnemiesToWorld(const LevelRuntime& runtime, WorldState& world) {
    world.flyingEnemyCount = runtime.flyingEnemyCount;
    for (uint8_t i = 0; i < MAX_FLYING_ENEMIES; ++i) {
        if (i < runtime.flyingEnemyCount && runtime.flyingEnemies[i].active) {
            const FlyingEnemy& enemy = runtime.flyingEnemies[i];
            world.flyingEnemies[i].x = enemy.x;
            world.flyingEnemies[i].y = enemy.y;
            world.flyingEnemies[i].facing = enemy.facing;
            world.flyingEnemies[i].wingPhase = enemy.wingPhase;
            world.flyingEnemies[i].active = 1;
        } else {
            world.flyingEnemies[i] = WorldState::SyncFlyingEnemy{};
        }
    }

    uint8_t projectileCount = 0;
    for (const TridentProjectile& projectile : runtime.projectiles) {
        if (projectile.active) {
            ++projectileCount;
        }
    }
    world.projectileCount = projectileCount;

    uint8_t outIndex = 0;
    for (const TridentProjectile& projectile : runtime.projectiles) {
        if (!projectile.active || outIndex >= MAX_TRIDENT_PROJECTILES) {
            continue;
        }
        world.projectiles[outIndex].x = projectile.x;
        world.projectiles[outIndex].y = projectile.y;
        world.projectiles[outIndex].rotation = projectile.rotation;
        world.projectiles[outIndex].active = 1;
        ++outIndex;
    }
    for (uint8_t i = outIndex; i < MAX_TRIDENT_PROJECTILES; ++i) {
        world.projectiles[i] = WorldState::SyncProjectile{};
    }
}

void initFlyingEnemiesForLevel(LevelRuntime& runtime, uint8_t globalLevelIndex, const std::string& tmxPath,
                               int tmxTileWidth) {
    runtime.flyingEnemyCount = 0;
    for (FlyingEnemy& enemy : runtime.flyingEnemies) {
        enemy = FlyingEnemy{};
    }
    for (TridentProjectile& projectile : runtime.projectiles) {
        projectile = TridentProjectile{};
    }

    if (globalLevelIndex != 1 || tmxPath.empty()) {
        return;
    }

    const std::vector<Vec2> spawns = loadFlyingEnemySpawnsFromTmx(tmxPath, tmxTileWidth);
    for (std::size_t i = 0; i < spawns.size() && i < MAX_FLYING_ENEMIES; ++i) {
        FlyingEnemy& enemy = runtime.flyingEnemies[i];
        enemy.anchorX = spawns[i].x;
        enemy.anchorY = spawns[i].y;
        enemy.x = enemy.anchorX;
        enemy.y = enemy.anchorY;
        enemy.facing = 1;
        enemy.shootTimer = ENEMY_INITIAL_SHOOT_DELAY + static_cast<float>(i) * 0.6f;
        enemy.wingPhase = static_cast<uint8_t>(i);
        enemy.active = true;
        ++runtime.flyingEnemyCount;
    }
}

void updateFlyingEnemiesAndProjectiles(LevelRuntime& runtime, const GameMap& map, WorldState& world, float dt) {
    if (runtime.flyingEnemyCount == 0) {
        syncFlyingEnemiesToWorld(runtime, world);
        return;
    }

    for (uint8_t i = 0; i < runtime.flyingEnemyCount; ++i) {
        FlyingEnemy& enemy = runtime.flyingEnemies[i];
        if (!enemy.active) {
            continue;
        }

        enemy.x += static_cast<float>(enemy.facing) * ENEMY_PATROL_SPEED * dt;
        if (enemy.x <= enemy.anchorX - ENEMY_PATROL_HALF_WIDTH) {
            enemy.x = enemy.anchorX - ENEMY_PATROL_HALF_WIDTH;
            enemy.facing = 1;
        } else if (enemy.x >= enemy.anchorX + ENEMY_PATROL_HALF_WIDTH) {
            enemy.x = enemy.anchorX + ENEMY_PATROL_HALF_WIDTH;
            enemy.facing = -1;
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f) {
            if (PlayerState* target = nearestAlivePlayer(world, enemy.x, enemy.y)) {
                shootTridentAtPlayer(enemy, runtime, *target);
            }
            enemy.shootTimer = TRIDENT_SHOOT_INTERVAL;
        }

        enemy.wingAnimTimer += dt;
        while (enemy.wingAnimTimer >= 0.14f) {
            enemy.wingAnimTimer -= 0.14f;
            enemy.wingPhase = static_cast<uint8_t>((enemy.wingPhase + 1) % 4);
        }
    }

    const float mapW = static_cast<float>(map.width()) * TILE_SIZE;
    const float mapH = static_cast<float>(map.height()) * TILE_SIZE;
    for (TridentProjectile& projectile : runtime.projectiles) {
        if (!projectile.active) {
            continue;
        }
        projectile.x += projectile.vx * dt;
        projectile.y += projectile.vy * dt;
        if (projectile.x < -TRIDENT_HITBOX || projectile.x > mapW + TRIDENT_HITBOX || projectile.y < -TRIDENT_HITBOX ||
            projectile.y > mapH + TRIDENT_HITBOX || projectileHitsSolid(map, projectile)) {
            projectile.active = false;
        }
    }

    syncFlyingEnemiesToWorld(runtime, world);
}

bool sampleFlyingEnemyHazard(const LevelRuntime& runtime, const PlayerState& player) {
    if (!player.alive) {
        return false;
    }

    const AABB playerBox = playerBounds(player);
    for (uint8_t i = 0; i < runtime.flyingEnemyCount; ++i) {
        const FlyingEnemy& enemy = runtime.flyingEnemies[i];
        if (!enemy.active) {
            continue;
        }
        if (playerBox.intersects(flyingEnemyHitbox(enemy))) {
            return true;
        }
    }
    return false;
}

bool sampleProjectileHazard(const LevelRuntime& runtime, const PlayerState& player) {
    if (!player.alive) {
        return false;
    }

    const AABB playerBox{player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT};
    for (const TridentProjectile& projectile : runtime.projectiles) {
        if (!projectile.active) {
            continue;
        }
        if (playerBox.intersects(tridentHitbox(projectile))) {
            return true;
        }
    }
    return false;
}

}  // namespace fireice
