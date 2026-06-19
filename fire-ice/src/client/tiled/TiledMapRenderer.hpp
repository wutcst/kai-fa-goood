#pragma once

#include <SFML/Graphics.hpp>

#include <functional>
#include <string>
#include <vector>

namespace fireice {

class TiledMapRenderer {
public:
    bool load(const std::string& tmxPath);
    void bake();  // 预渲染静态层到纹理，运行时只 draw 一次
    void drawStatic(sf::RenderWindow& window, const std::function<bool(int, int)>& skipTile = {}) const;
    void drawCollectibles(sf::RenderWindow& window, uint32_t collectedMask, uint32_t collectedMaskHi = 0,
                          uint32_t collectedMaskExt = 0) const;
    void drawPreview(sf::RenderWindow& window, const sf::FloatRect& area) const;  // 选关缩略图

    bool ready() const { return isLoaded_; }
    int mapWidth() const { return mapWidth_; }
    int mapHeight() const { return mapHeight_; }

private:
    struct TilesetRef {
        int firstGid = 0;
        int tileWidth = 32;
        int tileHeight = 32;
        int columns = 0;
        sf::Texture texture;
    };

    struct Layer {
        std::string name;
        std::vector<int> gids;
    };

    struct ImageLayer {
        sf::Texture texture;
        int imageWidth = 0;
        int imageHeight = 0;
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
    void drawLayerToTarget(sf::RenderTexture& target, const Layer& layer) const;
    void drawImageLayersToTarget(sf::RenderTexture& target, float scale) const;
    void drawImageLayersToWindow(sf::RenderWindow& window) const;
    void drawObjectTilesToTarget(sf::RenderTexture& target) const;
    void drawGidSprite(sf::RenderTexture& target, int gid, float x, float y, float width, float height) const;
    void drawGidSpriteToWindow(sf::RenderWindow& window, int gid, float x, float y, float width, float height) const;
    void drawTileLayersToWindow(sf::RenderWindow& window, const std::function<bool(int, int)>& skipTile) const;
    void drawObjectTilesToWindow(sf::RenderWindow& window) const;
    static bool isSpawnObjectName(const std::string& name);
    static int decodeGid(int rawGid);

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
    sf::RenderTexture bakedTexture_;
    sf::Sprite bakedSprite_;
    bool isLoaded_ = false;
    bool isBaked_ = false;
};

}  // namespace fireice
