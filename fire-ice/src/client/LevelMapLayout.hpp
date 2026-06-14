#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>

namespace fireice {

struct LevelNodeLayout {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr float kLevelNodeRadius = 26.0f;
constexpr float kBossNodeScale = 2.2f;
constexpr float kMapPanelLeft = 24.0f;
constexpr float kMapPanelTop = 88.0f;
constexpr float kMapPanelWidth = 976.0f;
constexpr float kMapPanelHeight = 480.0f;
constexpr float kMapHeaderHeight = 44.0f;
constexpr float kMapContentPadding = 16.0f;
constexpr float kMapScrollStep = 36.0f;

inline sf::FloatRect levelMapPanelRect() {
    return {kMapPanelLeft, kMapPanelTop, kMapPanelWidth, kMapPanelHeight};
}

inline sf::FloatRect levelMapViewportRect() {
    return {
        kMapPanelLeft + kMapContentPadding,
        kMapPanelTop + kMapHeaderHeight,
        kMapPanelWidth - kMapContentPadding * 2.0f,
        kMapPanelHeight - kMapHeaderHeight - kMapContentPadding,
    };
}

inline float levelMapContentHeight() {
    return 540.0f;
}

inline float levelMapMaxScroll() {
    const sf::FloatRect viewport = levelMapViewportRect();
    return std::max(0.0f, levelMapContentHeight() - viewport.height);
}

inline float clampLevelMapScroll(float scrollY) {
    return std::clamp(scrollY, 0.0f, levelMapMaxScroll());
}

inline const std::array<LevelNodeLayout, 8>& levelNodeLocalLayout() {
    static const std::array<LevelNodeLayout, 8> nodes = {{
        {326.0f, 470.0f},
        {180.0f, 400.0f},
        {480.0f, 350.0f},
        {140.0f, 280.0f},
        {326.0f, 230.0f},
        {520.0f, 170.0f},
        {200.0f, 110.0f},
        {472.0f, 62.0f},
    }};
    return nodes;
}

inline bool isFinalLevelNode(uint8_t index, uint8_t levelCount) {
    return levelCount > 0 && index + 1 == levelCount;
}

inline float levelNodeRadius(uint8_t index, uint8_t levelCount) {
    return isFinalLevelNode(index, levelCount) ? kLevelNodeRadius * kBossNodeScale : kLevelNodeRadius;
}

inline sf::Vector2f levelNodeLocalCenter(uint8_t index) {
    const auto& nodes = levelNodeLocalLayout();
    const std::size_t i = index < nodes.size() ? index : 0;
    return {nodes[i].x, nodes[i].y};
}

inline sf::FloatRect levelNodeLocalHitArea(uint8_t index, uint8_t levelCount) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    const float radius = levelNodeRadius(index, levelCount);
    return {center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f};
}

}  // namespace fireice
