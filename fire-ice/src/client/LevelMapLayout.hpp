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
constexpr float kMapPanelLeft = 24.0f;
constexpr float kMapPanelTop = 88.0f;
constexpr float kMapPanelWidth = 700.0f;
constexpr float kMapPanelHeight = 420.0f;
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
    return 520.0f;
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
        {326.0f, 40.0f},
    }};
    return nodes;
}

inline sf::Vector2f levelNodeLocalCenter(uint8_t index) {
    const auto& nodes = levelNodeLocalLayout();
    const std::size_t i = index < nodes.size() ? index : 0;
    return {nodes[i].x, nodes[i].y};
}

inline sf::FloatRect levelNodeLocalHitArea(uint8_t index) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    return {center.x - kLevelNodeRadius, center.y - kLevelNodeRadius, kLevelNodeRadius * 2.0f,
        kLevelNodeRadius * 2.0f};
}

} // namespace fireice
