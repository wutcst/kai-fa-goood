#include "Physics.hpp"

#include "LevelMechanics.hpp"

#include <algorithm>
#include <cmath>

namespace fireice {

namespace {

bool oneWayBlocks(const PlayerState& player, int ty) {
    const float tileTop = ty * TILE_SIZE;
    const float tileBottom = tileTop + TILE_SIZE;
    const float feet = player.y + PLAYER_HEIGHT;

    if (player.vy < 0.0f && feet <= tileTop + 2.0f) {
        return false;
    }
    if (feet <= tileTop + 1.0f) {
        return false;
    }
    if (player.y >= tileBottom) {
        return false;
    }
    return player.vy >= 0.0f;
}

bool tileBlocks(const GameMap& map, int tx, int ty, PlayerRole role, const WorldState& world, bool horizontal,
                const PlayerState& player) {
    const TileType type = map.tileAt(tx, ty);
    if (type == TileType::VanishingPlatform) {
        const int16_t slot = map.vanishingSlotAt(tx, ty);
        if (slot >= 0 && isVanishingTileHidden(world, static_cast<uint16_t>(slot))) {
            return false;
        }
        if (horizontal) {
            return false;
        }
        return oneWayBlocks(player, ty);
    }
    if (type == TileType::OneWayPlatform) {
        if (horizontal) {
            return false;
        }
        return oneWayBlocks(player, ty);
    }
    return map.blocksPlayer(type, role, world.fireDoorOpen, world.waterDoorOpen, world.poisonDoorOpen);
}

void resolveAxis(PlayerState& player, const GameMap& map, const WorldState& world, bool horizontal) {
    // 逐格检测 AABB 与 solid 重叠，沿单轴推出
    AABB box = playerBounds(player);

    const int minX = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((box.right() - 0.01f) / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((box.bottom() - 0.01f) / TILE_SIZE));

    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (!tileBlocks(map, tx, ty, player.role, world, horizontal, player)) {
                continue;
            }

            const float tileLeft = tx * TILE_SIZE;
            const float tileRight = tileLeft + TILE_SIZE;
            const float tileTop = ty * TILE_SIZE;
            const float tileBottom = tileTop + TILE_SIZE;

            if (horizontal) {
                if (player.vx > 0.0f) {
                    player.x = tileLeft - PLAYER_WIDTH;
                } else if (player.vx < 0.0f) {
                    player.x = tileRight;
                }
            } else {
                if (player.vy > 0.0f) {
                    player.y = tileTop - PLAYER_HEIGHT;
                    player.vy = 0.0f;
                    player.onGround = true;
                } else if (player.vy < 0.0f) {
                    player.y = tileBottom;
                    player.vy = 0.0f;
                }
            }

            box = playerBounds(player);
        }
    }
}

bool overlapsTile(const AABB& box, int tx, int ty) {
    const AABB tileBox{tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
    return box.intersects(tileBox);
}

}  // namespace

AABB playerBounds(const PlayerState& player) {
    return {player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT};
}

// 宝石拾取判定区域，比碰撞盒更宽更高
AABB playerCollectBounds(const PlayerState& player) {
    constexpr float kCollectWidth = TILE_SIZE * 1.35f;
    constexpr float kCollectHeight = TILE_SIZE * 2.25f;
    const float x = player.x + (PLAYER_WIDTH - kCollectWidth) * 0.5f;
    const float y = player.y + PLAYER_HEIGHT - kCollectHeight;
    return {x, y, kCollectWidth, kCollectHeight};
}

void applyInput(PlayerState& player, InputFlags input, float dt, bool groundJump, bool airJump,
                bool& airJumpUsedThisHold) {
    if (!player.alive) {
        return;
    }

    float move = 0.0f;
    if (hasFlag(input, InputFlags::Left)) {
        move -= 1.0f;
    }
    if (hasFlag(input, InputFlags::Right)) {
        move += 1.0f;
    }

    player.vx = move * MOVE_SPEED;

    if (groundJump) {
        player.vy = -JUMP_SPEED;
        player.onGround = false;
        player.airJumpsLeft = MAX_AIR_JUMPS;
        airJumpUsedThisHold = false;
    } else if (airJump && !player.onGround && player.airJumpsLeft > 0 && !airJumpUsedThisHold) {
        player.vy = -JUMP_SPEED;
        --player.airJumpsLeft;
        airJumpUsedThisHold = true;
    }

    if (hasFlag(input, InputFlags::Down) && !player.onGround) {
        player.vy = std::min(player.vy + GRAVITY * 2.0f * dt, MAX_FALL_SPEED);
    }

    (void) dt;
}

namespace {

// 低速跨越 tile 边界时吸附落地，减少边缘抖动
bool snapPlayerToGround(PlayerState& player, const GameMap& map, const WorldState& world) {
    if (!player.alive || player.vy > 80.0f) {
        return false;
    }

    const float probeX = player.x + PLAYER_WIDTH * 0.5f;
    const float feet = player.y + PLAYER_HEIGHT;
    const int tx = static_cast<int>(std::floor(probeX / TILE_SIZE));
    const int ty = static_cast<int>(std::floor((feet + 2.0f) / TILE_SIZE));
    const float tileTop = static_cast<float>(ty) * TILE_SIZE;

    if (feet < tileTop - 3.0f || feet > tileTop + 6.0f) {
        return false;
    }

    const TileType type = map.tileAt(tx, ty);
    if (type == TileType::Empty || type == TileType::Gem) {
        return false;
    }
    if (type == TileType::OneWayPlatform || type == TileType::VanishingPlatform) {
        return false;
    }
    if (!map.blocksPlayer(type, player.role, world.fireDoorOpen, world.waterDoorOpen, world.poisonDoorOpen)) {
        return false;
    }

    player.y = tileTop - PLAYER_HEIGHT;
    player.vy = 0.0f;
    player.onGround = true;
    return true;
}

}  // namespace

void integratePlayer(PlayerState& player, const GameMap& map, WorldState& world, float dt) {
    if (!player.alive) {
        return;
    }

    player.onGround = false;
    player.vy = std::min(player.vy + GRAVITY * dt, MAX_FALL_SPEED);

    // 先水平后垂直，分轴碰撞解析
    player.x += player.vx * dt;
    resolveAxis(player, map, world, true);

    player.y += player.vy * dt;
    resolveAxis(player, map, world, false);

    if (!player.onGround) {
        snapPlayerToGround(player, map, world);
    }

    if (player.onGround) {
        player.airJumpsLeft = 0;
    }

    if (player.y < -TILE_SIZE * 2.0f || player.y > map.height() * TILE_SIZE + TILE_SIZE * 4.0f) {
        player.alive = false;
    }
}

void applyFanZones(PlayerState& player, const std::vector<FanZone>& fans, float dt) {
    (void) dt;
    if (!player.alive || fans.empty()) {
        return;
    }

    const AABB box = playerBounds(player);
    for (const FanZone& fan : fans) {
        const AABB wind{fan.left, fan.top, fan.width, fan.height};
        if (!box.intersects(wind)) {
            continue;
        }

        player.onGround = false;
        if (player.y > fan.targetFeetY + 1.0f) {
            player.vy = -FAN_RISE_SPEED;
        } else {
            player.y = fan.targetFeetY;
            player.vy = 0.0f;
            player.onGround = true;
        }
        break;
    }
}

bool sampleHazard(const GameMap& map, const PlayerState& player, const WorldState& world) {
    (void) world;
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
            const TileType type = map.tileAt(tx, ty);
            if (map.isHazardFor(type, player.role) && overlapsTile(box, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

bool sampleExit(const GameMap& map, const PlayerState& player) {
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
            const TileType type = map.tileAt(tx, ty);
            if (map.isExitFor(type, player.role) && overlapsTile(box, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

void collectGems(PlayerState& player, GameMap& map) {
    if (!player.alive) {
        return;
    }

    const AABB box = playerCollectBounds(player);
    const int minX = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((box.right() - 0.01f) / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((box.bottom() - 0.01f) / TILE_SIZE));

    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (map.tileAt(tx, ty) != TileType::Gem || !overlapsTile(box, tx, ty)) {
                continue;
            }
            map.setTile(tx, ty, TileType::Empty);
            ++player.gems;
        }
    }
}

void updateButtons(const GameMap& map, WorldState& world) {
    // 任意存活玩家站在按钮上即开门，离开则关
    bool firePressed = false;
    bool waterPressed = false;
    bool poisonPressed = false;

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (map.tileAt(x, y) != TileType::Button) {
                continue;
            }

            const AABB buttonBox{x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            for (const PlayerState& player : world.players) {
                if (!player.alive) {
                    continue;
                }
                if (!playerCollectBounds(player).intersects(buttonBox)) {
                    continue;
                }
                if (player.role == PlayerRole::Fire) {
                    firePressed = true;
                } else if (player.role == PlayerRole::Water) {
                    waterPressed = true;
                } else if (player.role == PlayerRole::Poison) {
                    poisonPressed = true;
                }
            }
        }
    }

    world.fireDoorOpen = firePressed;
    world.waterDoorOpen = waterPressed;
    world.poisonDoorOpen = poisonPressed;
}

}  // namespace fireice
