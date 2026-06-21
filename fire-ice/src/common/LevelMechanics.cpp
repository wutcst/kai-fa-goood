#include "LevelMechanics.hpp"

#include "Paths.hpp"
#include "Physics.hpp"
#include "Pickup.hpp"

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

bool rockTouchesSolid(const GameMap& map, const RockHeadTrap& rock, float nextX, float nextY) {
    const AABB box{nextX + 2.0f, nextY + 2.0f, std::max(1.0f, rock.width - 4.0f), std::max(1.0f, rock.height - 4.0f)};
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
    world.rockHeadCount = static_cast<uint8_t>(std::min(runtime.rockHeads.size(), static_cast<std::size_t>(MAX_ROCK_HEAD_TRAPS)));
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

}  // namespace

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
            if ((rock.dirX < 0 && nextX <= rock.minX) || (rock.dirX > 0 && nextX >= rock.maxX) ||
                rockTouchesSolid(map, rock, nextX, rock.y)) {
                hit = true;
                nextX = std::clamp(nextX, rock.minX, rock.maxX);
            }
        } else if (rock.dirY != 0) {
            if ((rock.dirY < 0 && nextY <= rock.minY) || (rock.dirY > 0 && nextY >= rock.maxY) ||
                rockTouchesSolid(map, rock, rock.x, nextY)) {
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
    world.pendulumCount = static_cast<uint8_t>(std::min(runtime.pendulums.size(), static_cast<std::size_t>(MAX_PENDULUM_TRAPS)));
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

    std::sort(balls.begin(), balls.end(), [](const ObjectInfo& a, const ObjectInfo& b) { return a.center.x < b.center.x; });
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
        std::sort(candidates.begin(), candidates.end(), [](const ObjectInfo& a, const ObjectInfo& b) {
            return a.center.y < b.center.y;
        });
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
        const auto rightmost = std::max_element(rocks.begin(), rocks.end(), [](const RockHeadTrap& a, const RockHeadTrap& b) {
            return a.baseX < b.baseX;
        });
        for (RockHeadTrap& rock : rocks) {
            if (&rock == &(*rightmost)) {
                rock.dirY = -1;
                rock.startDirY = rock.dirY;
                rock.minX = rock.maxX = rock.x;
                rock.minY = rock.y - TILE_SIZE * 4.0f;
                rock.maxY = rock.y + TILE_SIZE * 4.0f;
            } else {
                rock.dirX = rock.baseX < rightmost->baseX ? 1 : -1;
                rock.startDirX = rock.dirX;
                rock.minX = rock.x - TILE_SIZE * 3.5f;
                rock.maxX = rock.x + TILE_SIZE * 3.5f;
                rock.minY = rock.maxY = rock.y;
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
