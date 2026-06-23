#include "CharacterAnimator.hpp"

#include "ClientVisuals.hpp"
#include "Paths.hpp"
#include "Physics.hpp"

#include <cmath>

namespace fireice {

namespace {

constexpr int kFrameW = 32;
constexpr int kFrameH = 32;
constexpr int kIdleFps = 8;
constexpr int kRunFps = 12;
constexpr int kHitFps = 10;

enum class CharAnim { Idle = 0, Run = 1, Jump = 2, Fall = 3, Hit = 4 };

int fpsForAnim(CharAnim anim) {
    switch (anim) {
        case CharAnim::Run:
            return kRunFps;
        case CharAnim::Hit:
            return kHitFps;
        default:
            return kIdleFps;
    }
}

// 带滞后的动画状态机，避免空中/地面状态频繁切换
CharAnim pickStableAnim(const PlayerState& player, int& airborneFrames, int& stableAnim, InputFlags moveHint) {
    if (!player.alive) {
        airborneFrames = 0;
        stableAnim = static_cast<int>(CharAnim::Hit);
        return CharAnim::Hit;
    }

    if (!player.onGround) {
        ++airborneFrames;
    } else {
        airborneFrames = 0;
    }

    const bool wantsMove = hasFlag(moveHint, InputFlags::Left) || hasFlag(moveHint, InputFlags::Right);
    CharAnim target = static_cast<CharAnim>(stableAnim);
    if (airborneFrames >= 2 || (!player.onGround && std::abs(player.vy) > 60.0f)) {
        target = player.vy < -40.0f ? CharAnim::Jump : CharAnim::Fall;
    } else if (std::abs(player.vx) > 35.0f ||
               (stableAnim == static_cast<int>(CharAnim::Run) && (std::abs(player.vx) > 12.0f || wantsMove)) ||
               (wantsMove && player.onGround)) {
        target = CharAnim::Run;
    } else if (std::abs(player.vx) < 8.0f && !wantsMove) {
        target = CharAnim::Idle;
    }

    stableAnim = static_cast<int>(target);
    return target;
}

// 朝向优先取输入方向，否则根据水平速度推断
int pickFacingDir(const PlayerState& player, InputFlags facingHint, int currentFacing) {
    if (hasFlag(facingHint, InputFlags::Left) && !hasFlag(facingHint, InputFlags::Right)) {
        return -1;
    }
    if (hasFlag(facingHint, InputFlags::Right) && !hasFlag(facingHint, InputFlags::Left)) {
        return 1;
    }
    if (player.vx > 20.0f) {
        return 1;
    }
    if (player.vx < -20.0f) {
        return -1;
    }
    return currentFacing;
}

}  // namespace

bool CharacterAnimator::loadSheet(const std::string& path, AnimSheet& out) {
    if (!out.texture.loadFromFile(resolveAssetPath(path))) {
        return false;
    }
    out.texture.setSmooth(false);
    out.frameCount = static_cast<int>(out.texture.getSize().x / static_cast<unsigned>(kFrameW));
    return out.frameCount > 0;
}

bool CharacterAnimator::load(const std::string& folderPath) {
    ready_ = loadSheet(folderPath + "/Idle (32x32).png", idle_) && loadSheet(folderPath + "/Run (32x32).png", run_) &&
             loadSheet(folderPath + "/Jump (32x32).png", jump_) && loadSheet(folderPath + "/Fall (32x32).png", fall_) &&
             loadSheet(folderPath + "/Hit (32x32).png", hit_);
    return ready_;
}

void CharacterAnimator::draw(sf::RenderWindow& window, const PlayerState& player, float animTime, InputFlags facingHint,
                             float footLift) const {
    if (!ready_) {
        return;
    }

    facingDir_ = pickFacingDir(player, facingHint, facingDir_);
    const CharAnim anim = pickStableAnim(player, airborneFrames_, stableAnim_, facingHint);

    const AnimSheet* sheet = &idle_;
    switch (anim) {
        case CharAnim::Run:
            sheet = &run_;
            break;
        case CharAnim::Jump:
            sheet = &jump_;
            break;
        case CharAnim::Fall:
            sheet = &fall_;
            break;
        case CharAnim::Hit:
            sheet = &hit_;
            break;
        case CharAnim::Idle:
        default:
            break;
    }
    if (sheet->frameCount <= 0) {
        return;
    }

    int frame = 0;
    if (sheet->frameCount > 1) {
        float animFps = static_cast<float>(fpsForAnim(anim));
        if (anim == CharAnim::Run) {
            const float speedRatio = std::clamp(std::abs(player.vx) / MOVE_SPEED, 0.2f, 1.0f);
            animFps *= speedRatio;
        }
        frame = static_cast<int>(animTime * animFps) % sheet->frameCount;
    }

    const float visualH = PLAYER_VISUAL_HEIGHT;
    const float visualW = visualH;
    const float visualX = player.x + (PLAYER_WIDTH - visualW) * 0.5f;
    const float visualY = player.y + PLAYER_HEIGHT - visualH - footLift;
    const float scale = visualH / static_cast<float>(kFrameH);
    const bool flip = facingDir_ < 0;

    sf::Sprite sprite(sheet->texture);
    sprite.setTextureRect(sf::IntRect(frame * kFrameW, 0, kFrameW, kFrameH));
    sprite.setScale(flip ? -scale : scale, scale);
    sprite.setPosition(flip ? visualX + visualW : visualX, visualY);

    if (!player.alive) {
        sprite.setColor(sf::Color(120, 120, 120, 140));
    }
    window.draw(sprite);
}

void CharacterAnimator::drawPortrait(sf::RenderWindow& window, float x, float y, float targetHeight, bool flipX,
                                     sf::Color tint) const {
    drawPortraitAnimated(window, x, y, targetHeight, flipX, 0.0f, tint);
}

void CharacterAnimator::drawPortraitAnimated(sf::RenderWindow& window, float x, float y, float targetHeight, bool flipX,
                                             float animTime, sf::Color tint) const {
    if (!ready_ || idle_.frameCount <= 0) {
        return;
    }

    int frame = 0;
    if (idle_.frameCount > 1) {
        frame = static_cast<int>(animTime * static_cast<float>(kIdleFps)) % idle_.frameCount;
    }

    const float scale = targetHeight / static_cast<float>(kFrameH);
    const float drawW = static_cast<float>(kFrameW) * scale;

    sf::Sprite sprite(idle_.texture);
    sprite.setTextureRect(sf::IntRect(frame * kFrameW, 0, kFrameW, kFrameH));
    sprite.setScale(flipX ? -scale : scale, scale);
    sprite.setPosition(flipX ? x + drawW : x, y);
    sprite.setColor(tint);
    window.draw(sprite);
}

}  // namespace fireice
