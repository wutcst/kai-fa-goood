#pragma once

#include "Types.hpp"

namespace fireice {

// 仅影响客户端渲染，不改变碰撞与关卡逻辑。
constexpr float PLAYER_VISUAL_HEIGHT = TILE_SIZE * 2.5f;
// Tiled 地形格顶面与精灵脚点对齐时的上移补偿（仅客户端绘制）
constexpr float TILED_GROUND_FOOT_LIFT = 6.0f;
constexpr float COLLECTIBLE_VISUAL_SCALE = 1.2f;
constexpr float GEM_VISUAL_SCALE = 1.35f;
constexpr float MUD_VISUAL_SCALE = 1.35f;

inline void scaleVisualFromBottom(float& x, float& y, float& width, float& height, float scaleX, float scaleY) {
    const float bottom = y + height;
    const float centerX = x + width * 0.5f;
    width *= scaleX;
    height *= scaleY;
    x = centerX - width * 0.5f;
    y = bottom - height;
}

}  // namespace fireice
