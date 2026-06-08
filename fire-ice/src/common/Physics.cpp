#include "Physics.hpp"

#include <algorithm>
#include <cmath>

namespace fireice {

namespace {

bool tileBlocks(const GameMap& map, int tx, int ty, PlayerRole role, const WorldState& world) {
    const TileType type = map.tileAt(tx, ty);
    return map.blocksPlayer(type, role, world.fireDoorOpen, world.waterDoorOpen, world.poisonDoorOpen);
}

void resolveAxis(PlayerState& player, const GameMap& map, const WorldState& world, bool horizontal) {
    AABB box = playerBounds(player);

    const int minX = static_cast<int>(std::floor(box.left() / TILE_SIZE));
    const int maxX = static_cast<int>(std::floor((box.right() - 0.01f) / TILE_SIZE));
    const int minY = static_cast<int>(std::floor(box.top() / TILE_SIZE));
    const int maxY = static_cast<int>(std::floor((box.bottom() - 0.01f) / TILE_SIZE));

    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (!tileBlocks(map, tx, ty, player.role, world)) {
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

} // namespace

AABB playerBounds(const PlayerState& player) {
    return {player.x, player.y, PLAYER_WIDTH, PLAYER_HEIGHT};
}

void applyInput(PlayerState& player, InputFlags input, float dt) {
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

    if (hasFlag(input, InputFlags::Jump) && player.onGround) {
        player.vy = -JUMP_SPEED;
        player.onGround = false;
    }

    if (hasFlag(input, InputFlags::Down) && !player.onGround) {
        player.vy = std::min(player.vy + GRAVITY * 2.0f * dt, MAX_FALL_SPEED);
    }

    (void)dt;
}

void integratePlayer(PlayerState& player, const GameMap& map, WorldState& world, float dt) {
    if (!player.alive) {
        return;
    }

    player.onGround = false;
    player.vy = std::min(player.vy + GRAVITY * dt, MAX_FALL_SPEED);

    player.x += player.vx * dt;
    resolveAxis(player, map, world, true);

    player.y += player.vy * dt;
    resolveAxis(player, map, world, false);

    if (player.y < -TILE_SIZE * 2.0f || player.y > map.height() * TILE_SIZE + TILE_SIZE * 4.0f) {
        player.alive = false;
    }
}

bool sampleHazard(const GameMap& map, const PlayerState& player, const WorldState& world) {
    (void)world;
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

    const AABB box = playerBounds(player);
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
                if (!playerBounds(player).intersects(buttonBox)) {
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

} // namespace fireice
