#pragma once

#include "Map.hpp"
#include "Types.hpp"

namespace fireice {

constexpr float PLAYER_WIDTH = TILE_SIZE * 0.7f;
constexpr float PLAYER_HEIGHT = TILE_SIZE * 1.3f;
constexpr uint8_t MAX_AIR_JUMPS = 1; // 空中可再跳次数（二段跳）

AABB playerBounds(const PlayerState& player);
void applyInput(PlayerState& player, InputFlags input, float dt, bool jumpPressed, bool jumpHeld,
    bool& airJumpUsedThisHold);
void integratePlayer(PlayerState& player, const GameMap& map, WorldState& world, float dt);
bool sampleHazard(const GameMap& map, const PlayerState& player, const WorldState& world);
bool sampleExit(const GameMap& map, const PlayerState& player);
void collectGems(PlayerState& player, GameMap& map);
void updateButtons(const GameMap& map, WorldState& world);

}  // namespace fireice
