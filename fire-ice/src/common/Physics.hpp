#pragma once

#include "Map.hpp"
#include "Types.hpp"

namespace fireice {

constexpr float PLAYER_WIDTH = TILE_SIZE * 0.5f;
constexpr float PLAYER_HEIGHT = TILE_SIZE * 0.75f;

AABB playerBounds(const PlayerState& player);
void applyInput(PlayerState& player, InputFlags input, float dt);
void integratePlayer(PlayerState& player, const GameMap& map, WorldState& world, float dt);
bool sampleHazard(const GameMap& map, const PlayerState& player, const WorldState& world);
bool sampleExit(const GameMap& map, const PlayerState& player);
void collectGems(PlayerState& player, GameMap& map);
void updateButtons(const GameMap& map, WorldState& world);

}  // namespace fireice
