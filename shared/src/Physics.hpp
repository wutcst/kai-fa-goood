#pragma once

#include "LevelMechanics.hpp"
#include "Map.hpp"
#include "Types.hpp"

namespace fireice {

constexpr float PLAYER_WIDTH = TILE_SIZE * 0.5f;
constexpr float PLAYER_HEIGHT = TILE_SIZE * 0.5f;
// 16px 图块缩放到 32px 后，薄平台可视高度约 12px（与 TiledMapRenderer 一致）
constexpr float THIN_PLATFORM_VISUAL_HEIGHT = TILE_SIZE * 0.5f;

float tileStandSurfaceY(TileType type, int ty);
constexpr uint8_t MAX_AIR_JUMPS = 1;  // 空中可再跳次数（二段跳）

AABB playerBounds(const PlayerState& player);
AABB playerCollectBounds(const PlayerState& player);
void applyInput(PlayerState& player, InputFlags input, float dt, bool groundJump, bool airJump,
                bool& airJumpUsedThisHold);
void integratePlayer(PlayerState& player, const GameMap& map, WorldState& world, float dt);
void applyFanZones(PlayerState& player, const std::vector<FanZone>& fans, float dt);
bool sampleHazard(const GameMap& map, const PlayerState& player, const WorldState& world);
bool sampleExit(const GameMap& map, const PlayerState& player);
void collectGems(PlayerState& player, GameMap& map);
void updateButtons(const GameMap& map, WorldState& world);

}  // namespace fireice
