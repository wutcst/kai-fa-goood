#pragma once

#include "Types.hpp"

#include <SFML/Graphics.hpp>
#include <string>

namespace fireice {

// 角色精灵表动画：从 Idle/Run/Jump/Fall/Hit 横条图切帧绘制
class CharacterAnimator {
public:
    bool load(const std::string& folderPath);
    bool ready() const { return ready_; }

    void draw(sf::RenderWindow& window, const PlayerState& player, float animTime,
              InputFlags facingHint = InputFlags::None, float footLift = 0.0f) const;
    void drawPortrait(sf::RenderWindow& window, float x, float y, float targetHeight, bool flipX,
                      sf::Color tint = sf::Color::White) const;
    void drawPortraitAnimated(sf::RenderWindow& window, float x, float y, float targetHeight, bool flipX,
                              float animTime, sf::Color tint = sf::Color::White) const;

private:
    struct AnimSheet {
        sf::Texture texture;
        int frameCount = 1;
    };

    bool loadSheet(const std::string& path, AnimSheet& out);

    AnimSheet idle_;
    AnimSheet run_;
    AnimSheet jump_;
    AnimSheet fall_;
    AnimSheet hit_;
    bool ready_ = false;

    mutable int facingDir_ = 1;
    mutable int stableAnim_ = 0;
    mutable int airborneFrames_ = 0;
};

}  // namespace fireice
