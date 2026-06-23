#include "AssetManager.hpp"
#include "Paths.hpp"

namespace fireice {

bool AssetManager::loadTexture(sf::Texture& texture, const std::string& fileName) {
    return texture.loadFromFile(resolveAssetPath("textures/" + fileName));
}

bool AssetManager::loadPixelTexture(sf::Texture& texture, const std::string& fileName) {
    if (!loadTexture(texture, fileName)) {
        return false;
    }
    texture.setSmooth(false);
    return true;
}

bool AssetManager::loadMapIcons() {
    for (std::size_t i = 0; i < mapLevelAvailable_.size(); ++i) {
        const std::string prefix = "map/level_" + std::to_string(i + 1) + "_";
        if (!loadPixelTexture(mapLevelAvailable_[i], prefix + "available.png") ||
            !loadPixelTexture(mapLevelLocked_[i], prefix + "locked.png") ||
            !loadPixelTexture(mapLevelCompleted_[i], prefix + "completed.png")) {
            return false;
        }
    }
    return loadPixelTexture(mapBossAvailable_, "map/boss_available.png") &&
           loadPixelTexture(mapBossLocked_, "map/boss_locked.png") &&
           loadPixelTexture(mapBossCompleted_, "map/boss_completed.png");
}

// 加载标题多层视差背景（由远及近，scrollSpeed 递增）
bool AssetManager::loadTitleParallax() {
    static const struct LayerDef {
        const char* fileName;
        float scrollSpeed;
    } kLayers[] = {
        {"title_parallax/sky.png", 6.f},
        {"title_parallax/far_mountains.png", 14.f},
        {"title_parallax/mountains.png", 28.f},
        {"title_parallax/trees.png", 46.f},
        {"title_parallax/foreground_trees.png", 68.f},
    };

    titleParallax_.clear();
    titleParallax_.reserve(std::size(kLayers));
    for (const LayerDef& layerDef : kLayers) {
        sf::Texture texture;
        if (!loadPixelTexture(texture, layerDef.fileName)) {
            titleParallax_.clear();
            return false;
        }
        titleParallax_.push_back(TitleParallaxLayer{std::move(texture), layerDef.scrollSpeed});
    }
    return true;
}

const CharacterAnimator& AssetManager::character(PlayerRole role) const {
    switch (role) {
        case PlayerRole::Water:
            return playerWater_;
        case PlayerRole::Poison:
            return playerPoison_;
        case PlayerRole::Fire:
        default:
            return playerFire_;
    }
}

const sf::Texture& AssetManager::mapLevelIcon(uint8_t levelIndex, bool unlocked, bool completed) const {
    const std::size_t iconIndex = levelIndex % mapLevelAvailable_.size();
    if (!unlocked) {
        return mapLevelLocked_[iconIndex];
    }
    if (completed) {
        return mapLevelCompleted_[iconIndex];
    }
    return mapLevelAvailable_[iconIndex];
}

const sf::Texture& AssetManager::mapBossIcon(bool unlocked, bool completed) const {
    if (!unlocked) {
        return mapBossLocked_;
    }
    if (completed) {
        return mapBossCompleted_;
    }
    return mapBossAvailable_;
}

bool AssetManager::load() {
    constexpr const char* kCharBase = "maps/tilesets/Main Characters";
    const bool charsOk = playerFire_.load(std::string(kCharBase) + "/Ninja Frog") &&
                         playerWater_.load(std::string(kCharBase) + "/Pink Man") &&
                         playerPoison_.load(std::string(kCharBase) + "/Mask Dude");

    titleParallaxReady_ = loadTitleParallax();

    ready_ = charsOk && loadPixelTexture(gemRed_, "red.png") && loadPixelTexture(gemBlue_, "blue.png") &&
             loadTexture(gameBackground_, "gamenew.jpg") && loadTexture(winScreen_, "winend.png") &&
             loadTexture(winScreen_, "winend.png") && loadTexture(winScreenPartial_, "winend2.png") &&
             loadTexture(loseScreen_, "loseend.png");

    buttonsReady_ = loadTexture(playButton_, "play2.png") && loadTexture(retryButton_, "retry_button.png") &&
                    loadTexture(continueButton_, "continue_button.png") && loadTexture(menuButton_, "menu_button.png");

    pauseIconReady_ = loadPixelTexture(pauseIcon_, "pause_icon.png");
    pauseMenuReady_ = loadTexture(pauseMenu_, "pause_menu.png");

    victoryUiReady_ = loadTexture(victoryMenu_, "victory/victory_menu.png") &&
                      loadTexture(victoryButtonMenu_, "victory/victory_button_menu.png") &&
                      loadTexture(victoryButtonRetry_, "victory/victory_button_retry.png") &&
                      loadTexture(victoryButtonNext_, "victory/victory_button_next.png");

    mapIconsReady_ = loadMapIcons();

    musicPath_ = resolveAssetPath("textures/LevelMusic.wav");
    return ready_;
}

}  // namespace fireice
