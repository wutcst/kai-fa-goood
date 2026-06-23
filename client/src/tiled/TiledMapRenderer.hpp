#pragma once

#include "Pickup.hpp"
#include "TmxUtil.hpp"

#include <SFML/Graphics.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fireice {

class TiledMapRenderer {
public:
    bool load(const std::string& tmxPath);
    void bake();  // 预渲染静态层到纹理，用于选关缩略图
    void drawStatic(sf::RenderWindow& window, const std::function<bool(int, int)>& skipTile = {}) const;
    void drawCollectibles(sf::RenderWindow& window, float animTimeSec, uint32_t collectedMask,
                          uint32_t collectedMaskHi = 0, uint32_t collectedMaskExt = 0,
                          const std::function<bool(uint8_t pickupIndex)>& skipPickup = {}) const;
    void drawObjectGidAt(sf::RenderWindow& window, int gid, float gameX, float gameY, float gameW, float gameH,
                         float animTimeSec) const;
    void drawAnimatedObjectGidAt(sf::RenderWindow& window, int gid, float gameX, float gameY, float gameW, float gameH,
                                 float animTimeSec) const;
    void drawPreview(sf::RenderWindow& window, const sf::FloatRect& area) const;  // 选关缩略图

    bool ready() const { return isLoaded_; }
    bool hasCustomBackground() const { return !imageLayers_.empty(); }
    int mapWidth() const { return mapWidth_; }
    int mapHeight() const { return mapHeight_; }
    // pickup 对象层中的可收集物数量（第一关结算 gemGoal 回退用）
    int collectibleCount() const { return static_cast<int>(collectibleObjectTiles_.size()); }

private:
    struct TileAnimation {
        std::vector<int> frames;
        std::vector<int> durationsMs;
        int totalMs = 0;
    };

    struct TilesetRef {
        int firstGid = 0;
        int tileWidth = 32;
        int tileHeight = 32;
        int columns = 0;
        sf::Texture texture;
        std::unordered_map<int, TileAnimation> animations;
        std::unordered_map<int, int> animationOwnerByFrame;
    };

    struct Layer {
        std::string name;
        std::vector<int> gids;
    };

    struct ImageLayer {
        sf::Texture texture;
        int imageWidth = 0;
        int imageHeight = 0;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        bool repeatX = false;
        bool repeatY = false;
    };

    struct CollectibleObjectTile {
        int gid = 0;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        uint8_t index = 0;
    };

    struct ObjectTile {
        int gid = 0;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    const TilesetRef* findTileset(int gid) const;
    void drawLayerToTarget(sf::RenderTexture& target, const Layer& layer,
                           const std::function<bool(int, int)>& excludeTile = {}) const;
    void drawImageLayersToTarget(sf::RenderTexture& target, float scale) const;
    void drawImageLayersToWindow(sf::RenderWindow& window) const;
    void drawObjectTilesToTarget(sf::RenderTexture& target) const;
    void drawGidSprite(sf::RenderTexture& target, int gid, float x, float y, float width, float height) const;
    void drawGidSpriteToWindow(sf::RenderWindow& window, int gid, float x, float y, float width, float height,
                               float animTimeSec = 0.0f) const;
    void drawTileLayersToWindow(sf::RenderWindow& window, const std::function<bool(int, int)>& skipTile) const;
    void drawObjectTilesToWindow(sf::RenderWindow& window) const;
    void drawCachedGrassToTarget(sf::RenderTexture& target, float mapPixelW, float mapPixelH) const;
    bool loadImageLayerTexture(const std::string& mapDir, const tmx::ImageLayerData& source, ImageLayer& out) const;
    void drawGrassUnderlay(sf::RenderWindow& window, float mapPixelW, float mapPixelH) const;
    void drawImageLayerRepeating(sf::RenderWindow& window, const ImageLayer& layer, float mapPixelW,
                                 float mapPixelH) const;
    static bool isSpawnObjectName(const std::string& name);
    static int decodeGid(int rawGid);
    static int resolveAnimatedLocalId(const TilesetRef& tileset, int localId, float animTimeSec);
    static void parseTileAnimationsFromTsx(const std::string& xml, TilesetRef& tileset);
    static void parseTileAnimationsFromTsj(const std::string& json, TilesetRef& tileset);
    static bool loadTilesetFromFile(const std::string& path, int firstGid, TilesetRef& out);
    static bool loadTilesetFromTsxFile(const std::string& tsxPath, int firstGid, TilesetRef& out);
    static bool loadTilesetFromTsjFile(const std::string& tsjPath, int firstGid, TilesetRef& out);

    std::vector<TmxTilesetInfo> tilesetInfo_;

    int mapWidth_ = 0;
    int mapHeight_ = 0;
    int tileWidth_ = 32;
    int tileHeight_ = 32;
    float mapScale_ = 1.0f;  // TMX 16px 图块 → 游戏 32px 的缩放比
    std::vector<TilesetRef> tilesets_;
    std::vector<Layer> visualLayers_;
    std::vector<ImageLayer> imageLayers_;
    std::vector<ObjectTile> objectTiles_;
    std::vector<CollectibleObjectTile> collectibleObjectTiles_;
    std::string mapDirectory_;
    ImageLayer cachedGrass_;
    bool hasCachedGrass_ = false;
    sf::RenderTexture bakedTexture_;
    sf::Sprite bakedSprite_;
    bool isLoaded_ = false;
    bool isBaked_ = false;
};

}  // namespace fireice
