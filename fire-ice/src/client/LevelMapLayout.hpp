#pragma once

#include "../common/Types.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace fireice {

struct LevelNodeLayout {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr float kLevelNodeRadius = 34.0f;
constexpr float kBossNodeScale = 2.35f;
constexpr float kLobbyDesignWidth = 1280.0f;
constexpr float kLobbyDesignHeight = 720.0f;
constexpr float kMapPanelLeft = 32.0f;
constexpr float kPreviewPanelWidth = 400.0f;
constexpr float kPreviewPanelMargin = 32.0f;
constexpr float kMapPreviewGap = 24.0f;
constexpr float kMapHeaderHeight = 52.0f;
constexpr float kMapContentPadding = 16.0f;
constexpr float kMapScrollStep = 36.0f;

inline float lobbyUiScale() {
    const float sx = static_cast<float>(LOBBY_WINDOW_WIDTH) / kLobbyDesignWidth;
    const float sy = static_cast<float>(LOBBY_WINDOW_HEIGHT) / kLobbyDesignHeight;
    return std::min(sx, sy);
}

inline float lobbyScaled(float value) {
    return value * lobbyUiScale();
}

inline float levelMapPanelTop() {
    return lobbyScaled(88.0f) + 12.0f;
}

inline float previewPanelWidth() {
    return lobbyScaled(kPreviewPanelWidth);
}

inline float levelMapPanelWidth() {
    return static_cast<float>(LOBBY_WINDOW_WIDTH) - lobbyScaled(kMapPanelLeft) - lobbyScaled(kMapPreviewGap) -
           previewPanelWidth() - lobbyScaled(kPreviewPanelMargin);
}

inline float levelMapPanelHeight() {
    const float bottomReserve = lobbyScaled(96.0f);
    return static_cast<float>(LOBBY_WINDOW_HEIGHT) - levelMapPanelTop() - bottomReserve;
}

inline float levelMapHeaderHeight() {
    return lobbyScaled(kMapHeaderHeight);
}

inline float levelMapContentPadding() {
    return lobbyScaled(kMapContentPadding);
}

inline sf::FloatRect levelMapPanelRect() {
    return {lobbyScaled(kMapPanelLeft), levelMapPanelTop(), levelMapPanelWidth(), levelMapPanelHeight()};
}

inline sf::FloatRect levelMapViewportRect() {
    const float pad = levelMapContentPadding();
    const float headerH = levelMapHeaderHeight();
    return {lobbyScaled(kMapPanelLeft) + pad, levelMapPanelTop() + headerH, levelMapPanelWidth() - pad * 2.0f,
            levelMapPanelHeight() - headerH - pad};
}

inline const std::array<LevelNodeLayout, 8>& levelNodeLocalLayout() {
    // 以 (315, 210) 为中心，加大之字形的横向摆幅与纵向间距
    static const std::array<LevelNodeLayout, 8> nodes = {{
        {315.0f, 720.0f},
        {90.0f, 570.0f},
        {540.0f, 420.0f},
        {80.0f, 270.0f},
        {315.0f, 120.0f},
        {530.0f, -30.0f},
        {95.0f, -180.0f},
        {315.0f, -330.0f},
    }};
    return nodes;
}

inline sf::FloatRect levelNodeLayoutBounds() {
    const auto& nodes = levelNodeLocalLayout();
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& node : nodes) {
        minX = std::min(minX, node.x);
        maxX = std::max(maxX, node.x);
        minY = std::min(minY, node.y);
        maxY = std::max(maxY, node.y);
    }
    const float margin = kLevelNodeRadius * kBossNodeScale + 24.0f;
    return {minX - margin, minY - margin, maxX - minX + margin * 2.0f, maxY - minY + margin * 2.0f};
}

inline float levelMapLayoutScale() {
    const sf::FloatRect viewport = levelMapViewportRect();
    const sf::FloatRect bounds = levelNodeLayoutBounds();
    const float pad = lobbyScaled(12.0f);
    const float scaleX = (viewport.width - pad * 2.0f) / bounds.width;
    // 按宽度撑满视口（约占 94%），纵向超出时靠滚动展示
    return scaleX * 0.94f;
}

inline sf::Vector2f levelNodeLayoutOffset() {
    const sf::FloatRect viewport = levelMapViewportRect();
    const sf::FloatRect bounds = levelNodeLayoutBounds();
    const float scale = levelMapLayoutScale();
    const float contentW = bounds.width * scale;
    const float contentH = bounds.height * scale;
    float offsetY;
    if (contentH <= viewport.height) {
        // 内容能完整显示时垂直居中
        offsetY = (viewport.height - contentH) / 2.0f - bounds.top * scale;
    } else {
        // 内容超出视口时顶对齐，使 scrollY=0 对应路线图顶端
        offsetY = -bounds.top * scale;
    }
    return {(viewport.width - contentW) / 2.0f - bounds.left * scale, offsetY};
}

inline float levelMapContentHeight() {
    // 实际内容高度（用于计算滚动条与最大滚动距离）
    return levelNodeLayoutBounds().height * levelMapLayoutScale();
}

inline float levelMapMaxScroll() {
    const sf::FloatRect viewport = levelMapViewportRect();
    return std::max(0.0f, levelMapContentHeight() - viewport.height);
}

inline float clampLevelMapScroll(float scrollY) {
    return std::clamp(scrollY, 0.0f, levelMapMaxScroll());
}

inline bool isFinalLevelNode(uint8_t index, uint8_t levelCount) {
    return levelCount > 0 && index + 1 == levelCount;
}

inline float levelNodeRadius(uint8_t index, uint8_t levelCount) {
    const float scale = levelMapLayoutScale();
    const float base = isFinalLevelNode(index, levelCount) ? kLevelNodeRadius * kBossNodeScale : kLevelNodeRadius;
    return base * scale;
}

inline sf::Vector2f levelNodeLocalCenter(uint8_t index) {
    const auto& nodes = levelNodeLocalLayout();
    const std::size_t i = index < nodes.size() ? index : 0;
    const float scale = levelMapLayoutScale();
    const sf::Vector2f offset = levelNodeLayoutOffset();
    return {offset.x + nodes[i].x * scale, offset.y + nodes[i].y * scale};
}

inline sf::FloatRect levelNodeLocalHitArea(uint8_t index, uint8_t levelCount) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    const float radius = levelNodeRadius(index, levelCount);
    return {center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f};
}

}  // namespace fireice
