#include "TiledMapRenderer.hpp"

#include "Paths.hpp"
#include "Pickup.hpp"
#include "TmxUtil.hpp"
#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fireice {

namespace {

struct TilesetLoadData {
    int firstGid = 0;
    int tileWidth = 32;
    int tileHeight = 32;
    int columns = 0;
    sf::Texture texture;
};

bool loadTilesetFromTsx(const std::string& tsxPath, int firstGid, TilesetLoadData& out) {
    const std::string xml = tmx::readFile(tsxPath);
    if (xml.empty()) {
        return false;
    }

    const auto tags = tmx::findTags(xml, "tileset");
    if (tags.empty()) {
        return false;
    }
    const std::string& header = tags.front();
    out.firstGid = firstGid;
    if (const auto tw = tmx::attributeValue(header, "tilewidth")) {
        out.tileWidth = std::stoi(*tw);
    }
    if (const auto th = tmx::attributeValue(header, "tileheight")) {
        out.tileHeight = std::stoi(*th);
    }
    if (const auto columns = tmx::attributeValue(header, "columns")) {
        out.columns = std::stoi(*columns);
    }

    const auto imageTags = tmx::findTags(xml, "image");
    if (imageTags.empty()) {
        return false;
    }
    const auto source = tmx::attributeValue(imageTags.front(), "source");
    if (!source) {
        return false;
    }

    const std::string imagePath = resolveAssetPath(tmx::joinPath(tmx::parentDirectory(tsxPath), *source));
    if (!out.texture.loadFromFile(imagePath)) {
        std::cerr << "[TiledMapRenderer] Failed to load tileset image: " << imagePath << std::endl;
        return false;
    }
    if (out.columns <= 0) {
        const unsigned texWidth = out.texture.getSize().x;
        out.columns = static_cast<int>(texWidth / static_cast<unsigned>(out.tileWidth));
    }
    return true;
}

bool loadTilesetFromTsj(const std::string& tsjPath, int firstGid, TilesetLoadData& out) {
    const std::string json = tmx::readFile(tsjPath);
    if (json.empty()) {
        return false;
    }

    out.firstGid = firstGid;
    if (const auto tw = tmx::jsonIntField(json, "tilewidth")) {
        out.tileWidth = *tw;
    }
    if (const auto th = tmx::jsonIntField(json, "tileheight")) {
        out.tileHeight = *th;
    }
    if (const auto columns = tmx::jsonIntField(json, "columns")) {
        out.columns = *columns;
    }

    const auto image = tmx::jsonStringField(json, "image");
    if (!image) {
        return false;
    }

    const std::string imagePath = resolveAssetPath(tmx::joinPath(tmx::parentDirectory(tsjPath), *image));
    if (!out.texture.loadFromFile(imagePath)) {
        std::cerr << "[TiledMapRenderer] Failed to load tileset image: " << imagePath << std::endl;
        return false;
    }
    if (out.columns <= 0) {
        const unsigned texWidth = out.texture.getSize().x;
        out.columns = static_cast<int>(texWidth / static_cast<unsigned>(out.tileWidth));
    }
    return true;
}

bool loadTilesetFromFile(const std::string& path, int firstGid, TilesetLoadData& out) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".tsj") {
        return loadTilesetFromTsj(path, firstGid, out);
    }
    return loadTilesetFromTsx(path, firstGid, out);
}

}  // namespace

bool TiledMapRenderer::isSpawnObjectName(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower == "player1" || lower == "player2" || lower == "player" || lower == "fire_spawn" ||
           lower == "water_spawn" || lower == "poison_spawn";
}

int TiledMapRenderer::decodeGid(int rawGid) {
    return rawGid & 0x1FFFFFFF;
}

bool TiledMapRenderer::load(const std::string& tmxPath) {
    isLoaded_ = false;
    isBaked_ = false;
    tilesets_.clear();
    visualLayers_.clear();
    imageLayers_.clear();
    objectTiles_.clear();
    collectibleObjectTiles_.clear();
    mapScale_ = 1.0f;

    const std::string resolvedTmx = resolveAssetPath(tmxPath);
    const std::string xml = tmx::readFile(resolvedTmx);
    if (xml.empty()) {
        std::cerr << "[TiledMapRenderer] Failed to read map: " << resolvedTmx << std::endl;
        return false;
    }

    const auto mapTags = tmx::findTags(xml, "map");
    if (mapTags.empty()) {
        return false;
    }
    const std::string& mapTag = mapTags.front();
    if (const auto w = tmx::attributeValue(mapTag, "width")) {
        mapWidth_ = std::stoi(*w);
    }
    if (const auto h = tmx::attributeValue(mapTag, "height")) {
        mapHeight_ = std::stoi(*h);
    }
    if (const auto tw = tmx::attributeValue(mapTag, "tilewidth")) {
        tileWidth_ = std::stoi(*tw);
    }
    if (const auto th = tmx::attributeValue(mapTag, "tileheight")) {
        tileHeight_ = std::stoi(*th);
    }

    if (tileWidth_ > 0 && tileWidth_ != static_cast<int>(TILE_SIZE)) {
        // 例如 TMX tilewidth=16，游戏逻辑 TILE_SIZE=32
        mapScale_ = TILE_SIZE / static_cast<float>(tileWidth_);
    }

    const std::string mapDir = tmx::parentDirectory(resolvedTmx);
    for (const std::string& tilesetTag : tmx::findTags(xml, "tileset")) {
        const auto source = tmx::attributeValue(tilesetTag, "source");
        if (!source) {
            continue;
        }
        const auto firstGid = tmx::attributeValue(tilesetTag, "firstgid");
        if (!firstGid) {
            continue;
        }

        TilesetLoadData loaded;
        const std::string tilesetPath = resolveAssetPath(tmx::joinPath(mapDir, *source));
        if (!loadTilesetFromFile(tilesetPath, std::stoi(*firstGid), loaded)) {
            continue;
        }
        TilesetRef tileset;
        tileset.firstGid = loaded.firstGid;
        tileset.tileWidth = loaded.tileWidth;
        tileset.tileHeight = loaded.tileHeight;
        tileset.columns = loaded.columns;
        tileset.texture = std::move(loaded.texture);
        tilesets_.push_back(std::move(tileset));
    }

    std::sort(tilesets_.begin(), tilesets_.end(),
              [](const TilesetRef& a, const TilesetRef& b) { return a.firstGid < b.firstGid; });

    for (const tmx::ImageLayerData& imageLayer : tmx::findAllImageLayers(xml)) {
        const std::filesystem::path imagePath =
            std::filesystem::path(mapDir) / std::filesystem::path(imageLayer.imageSource);
        const std::string resolvedImage = resolveAssetPath(imagePath.lexically_normal().string());
        ImageLayer loaded;
        loaded.imageWidth = imageLayer.imageWidth;
        loaded.imageHeight = imageLayer.imageHeight;
        loaded.repeatX = imageLayer.repeatX;
        loaded.repeatY = imageLayer.repeatY;
        if (!loaded.texture.loadFromFile(resolvedImage)) {
            std::cerr << "[TiledMapRenderer] Failed to load image layer: " << resolvedImage << std::endl;
            continue;
        }
        loaded.texture.setSmooth(false);
        const sf::Vector2u texSize = loaded.texture.getSize();
        if (loaded.imageWidth <= 0) {
            loaded.imageWidth = static_cast<int>(texSize.x);
        }
        if (loaded.imageHeight <= 0) {
            loaded.imageHeight = static_cast<int>(texSize.y);
        }
        imageLayers_.push_back(std::move(loaded));
    }

    for (const tmx::TileLayerData& tileLayer : tmx::findAllTileLayers(xml)) {
        if (static_cast<int>(tileLayer.gids.size()) != mapWidth_ * mapHeight_) {
            std::cerr << "[TiledMapRenderer] Layer size mismatch: " << tileLayer.name << std::endl;
            continue;
        }
        Layer layer;
        layer.name = tileLayer.name;
        layer.gids = tileLayer.gids;
        visualLayers_.push_back(std::move(layer));
    }

    uint8_t collectibleIndex = 0;
    for (const tmx::ObjectTileData& obj : tmx::findObjectTiles(xml)) {
        // 出生点 object 由服务端 collision 决定，视觉层跳过
        if (isSpawnObjectName(obj.name)) {
            continue;
        }
        if (obj.gid > 0) {
            const int decodedGid = decodeGid(obj.gid);
            if (decodedGid == 303) {
                continue;
            }
            if (collectibleIndex >= MAX_PICKUPS) {
                continue;
            }
            CollectibleObjectTile tile;
            tile.gid = obj.gid;
            tile.x = obj.x;
            tile.y = obj.y;
            tile.width = obj.width;
            tile.height = obj.height;
            tile.index = collectibleIndex++;
            collectibleObjectTiles_.push_back(std::move(tile));
            continue;
        }

        ObjectTile tile;
        tile.gid = obj.gid;
        tile.x = obj.x;
        tile.y = obj.y;
        tile.width = obj.width;
        tile.height = obj.height;
        objectTiles_.push_back(std::move(tile));
    }

    if (visualLayers_.empty()) {
        std::cerr << "[TiledMapRenderer] No tile layers found in map: " << resolvedTmx << std::endl;
    } else if (tilesets_.empty()) {
        std::cerr << "[TiledMapRenderer] No tilesets loaded for map: " << resolvedTmx << std::endl;
    }

    isLoaded_ = !tilesets_.empty() && !visualLayers_.empty();
    return isLoaded_;
}

const TiledMapRenderer::TilesetRef* TiledMapRenderer::findTileset(int gid) const {
    if (gid <= 0) {
        return nullptr;
    }
    const TilesetRef* chosen = nullptr;
    for (const TilesetRef& tileset : tilesets_) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    return chosen;
}

void TiledMapRenderer::drawGidSprite(sf::RenderTexture& target, int rawGid, float x, float y, float width,
                                     float height) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    const int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }

    const int columns = std::max(1, tileset->columns);
    const int rows = static_cast<int>(tileset->texture.getSize().y /
                                      static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float drawWidth = width > 0.0f ? width : static_cast<float>(tileset->tileWidth);
    const float drawHeight = height > 0.0f ? height : static_cast<float>(tileset->tileHeight);
    const float drawX = x;
    const float drawY = y - drawHeight;

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setPosition(drawX, drawY);
    if (drawWidth != static_cast<float>(tileset->tileWidth) || drawHeight != static_cast<float>(tileset->tileHeight)) {
        sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                        drawHeight / static_cast<float>(tileset->tileHeight));
    }
    target.draw(sprite);
}

void TiledMapRenderer::drawGidSpriteToWindow(sf::RenderWindow& window, int rawGid, float x, float y, float width,
                                             float height) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    const int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }

    const int columns = std::max(1, tileset->columns);
    const int rows = static_cast<int>(tileset->texture.getSize().y /
                                      static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float drawWidth =
        (width > 0.0f ? width : static_cast<float>(tileset->tileWidth)) * mapScale_;
    const float drawHeight =
        (height > 0.0f ? height : static_cast<float>(tileset->tileHeight)) * mapScale_;
    const float drawX = x * mapScale_;
    const float drawY = y * mapScale_ - drawHeight;

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setPosition(drawX, drawY);
    sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                    drawHeight / static_cast<float>(tileset->tileHeight));
    window.draw(sprite);
}

void TiledMapRenderer::drawLayerToTarget(sf::RenderTexture& target, const Layer& layer) const {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            const int gid = layer.gids[static_cast<std::size_t>(y * mapWidth_ + x)];
            if (gid <= 0) {
                continue;
            }
            drawGidSprite(target, gid, static_cast<float>(x * tileWidth_), static_cast<float>((y + 1) * tileHeight_),
                          static_cast<float>(tileWidth_), static_cast<float>(tileHeight_));
        }
    }
}

void TiledMapRenderer::drawImageLayersToTarget(sf::RenderTexture& target, float scale) const {
    const float mapPixelW = static_cast<float>(mapWidth_ * tileWidth_) * scale;
    const float mapPixelH = static_cast<float>(mapHeight_ * tileHeight_) * scale;

    for (const ImageLayer& layer : imageLayers_) {
        const sf::Vector2u texSize = layer.texture.getSize();
        if (texSize.x == 0 || texSize.y == 0) {
            continue;
        }

        const float stampW =
            static_cast<float>(layer.imageWidth > 0 ? layer.imageWidth : static_cast<int>(texSize.x)) * scale;
        const float stampH =
            static_cast<float>(layer.imageHeight > 0 ? layer.imageHeight : static_cast<int>(texSize.y)) * scale;
        const float endX = layer.repeatX ? mapPixelW : stampW;
        const float endY = layer.repeatY ? mapPixelH : stampH;

        for (float y = 0.0f; y < endY; y += stampH) {
            for (float x = 0.0f; x < endX; x += stampW) {
                sf::Sprite sprite(layer.texture);
                sprite.setScale(stampW / static_cast<float>(texSize.x), stampH / static_cast<float>(texSize.y));
                sprite.setPosition(x, y);
                target.draw(sprite);
            }
        }
    }
}

void TiledMapRenderer::drawImageLayersToWindow(sf::RenderWindow& window) const {
    const float mapPixelW = static_cast<float>(mapWidth_ * tileWidth_) * mapScale_;
    const float mapPixelH = static_cast<float>(mapHeight_ * tileHeight_) * mapScale_;

    for (const ImageLayer& layer : imageLayers_) {
        const sf::Vector2u texSize = layer.texture.getSize();
        if (texSize.x == 0 || texSize.y == 0) {
            continue;
        }

        const float stampW =
            static_cast<float>(layer.imageWidth > 0 ? layer.imageWidth : static_cast<int>(texSize.x)) * mapScale_;
        const float stampH =
            static_cast<float>(layer.imageHeight > 0 ? layer.imageHeight : static_cast<int>(texSize.y)) * mapScale_;
        const float endX = layer.repeatX ? mapPixelW : stampW;
        const float endY = layer.repeatY ? mapPixelH : stampH;

        for (float y = 0.0f; y < endY; y += stampH) {
            for (float x = 0.0f; x < endX; x += stampW) {
                sf::Sprite sprite(layer.texture);
                sprite.setScale(stampW / static_cast<float>(texSize.x), stampH / static_cast<float>(texSize.y));
                sprite.setPosition(x, y);
                window.draw(sprite);
            }
        }
    }
}

void TiledMapRenderer::drawObjectTilesToTarget(sf::RenderTexture& target) const {
    for (const ObjectTile& obj : objectTiles_) {
        drawGidSprite(target, obj.gid, obj.x, obj.y, obj.width, obj.height);
    }
}

void TiledMapRenderer::drawTileLayersToWindow(sf::RenderWindow& window,
                                              const std::function<bool(int, int)>& skipTile) const {
    for (const Layer& layer : visualLayers_) {
        for (int y = 0; y < mapHeight_; ++y) {
            for (int x = 0; x < mapWidth_; ++x) {
                if (skipTile && skipTile(x, y)) {
                    continue;
                }
                const int gid = layer.gids[static_cast<std::size_t>(y * mapWidth_ + x)];
                if (gid <= 0) {
                    continue;
                }
                drawGidSpriteToWindow(window, gid, static_cast<float>(x * tileWidth_),
                                      static_cast<float>((y + 1) * tileHeight_), static_cast<float>(tileWidth_),
                                      static_cast<float>(tileHeight_));
            }
        }
    }
}

void TiledMapRenderer::drawObjectTilesToWindow(sf::RenderWindow& window) const {
    for (const ObjectTile& obj : objectTiles_) {
        drawGidSpriteToWindow(window, obj.gid, obj.x, obj.y, obj.width, obj.height);
    }
}

void TiledMapRenderer::bake() {
    // 仅用于选关缩略图；游戏中通过 drawTileLayersToWindow 逐帧绘制
    isBaked_ = false;
    if (mapWidth_ <= 0 || mapHeight_ <= 0 || visualLayers_.empty()) {
        return;
    }

    const unsigned width = static_cast<unsigned>(mapWidth_ * tileWidth_);
    const unsigned height = static_cast<unsigned>(mapHeight_ * tileHeight_);
    if (!bakedTexture_.create(width, height)) {
        std::cerr << "[TiledMapRenderer] Failed to create render texture" << std::endl;
        return;
    }

    bakedTexture_.clear(sf::Color(60, 44, 32));
    for (const Layer& layer : visualLayers_) {
        drawLayerToTarget(bakedTexture_, layer);
    }
    drawObjectTilesToTarget(bakedTexture_);
    bakedTexture_.display();

    bakedSprite_.setTexture(bakedTexture_.getTexture());
    bakedSprite_.setPosition(0.0f, 0.0f);
    bakedSprite_.setScale(mapScale_, mapScale_);
    isBaked_ = true;
}

void TiledMapRenderer::drawStatic(sf::RenderWindow& window,
                                    const std::function<bool(int, int)>& skipTile) const {
    if (!isLoaded_) {
        return;
    }
    const float mapPixelW = static_cast<float>(mapWidth_) * TILE_SIZE;
    const float mapPixelH = static_cast<float>(mapHeight_) * TILE_SIZE;
    if (imageLayers_.empty()) {
        sf::RectangleShape bg({mapPixelW, mapPixelH});
        bg.setPosition(0.0f, 0.0f);
        bg.setFillColor(sf::Color(60, 44, 32));
        window.draw(bg);
    } else {
        drawImageLayersToWindow(window);
    }
    drawTileLayersToWindow(window, skipTile);
    drawObjectTilesToWindow(window);
}

void TiledMapRenderer::drawCollectibles(sf::RenderWindow& window, uint32_t collectedMask,
                                        uint32_t collectedMaskHi, uint32_t collectedMaskExt) const {
    for (const CollectibleObjectTile& obj : collectibleObjectTiles_) {
        const uint32_t bit = 1u << (obj.index % 32u);
        const uint8_t word = obj.index / 32u;
        const bool taken =
            word == 0 ? ((collectedMask & bit) != 0)
                      : (word == 1 ? ((collectedMaskHi & bit) != 0) : ((collectedMaskExt & bit) != 0));
        if (taken) {
            continue;
        }
        drawGidSpriteToWindow(window, obj.gid, obj.x, obj.y, obj.width, obj.height);
    }
}

void TiledMapRenderer::drawPreview(sf::RenderWindow& window, const sf::FloatRect& area) const {
    // 等比缩放并居中，用于选关右侧面板
    sf::RectangleShape backdrop({area.width, area.height});
    backdrop.setPosition(area.left, area.top);
    backdrop.setFillColor(sf::Color(24, 32, 28, 230));
    backdrop.setOutlineThickness(2.0f);
    backdrop.setOutlineColor(sf::Color(80, 70, 50));
    window.draw(backdrop);

    if (!isBaked_) {
        return;
    }

    const sf::Vector2u texSize = bakedTexture_.getSize();
    if (texSize.x == 0 || texSize.y == 0) {
        return;
    }

    const float mapW = static_cast<float>(texSize.x);
    const float mapH = static_cast<float>(texSize.y);
    const float scale = std::min((area.width - 12.0f) / mapW, (area.height - 28.0f) / mapH);
    const float drawW = mapW * scale;
    const float drawH = mapH * scale;

    sf::Sprite sprite(bakedTexture_.getTexture());
    sprite.setScale(scale, scale);
    sprite.setPosition(area.left + (area.width - drawW) / 2.0f,
                       area.top + 20.0f + (area.height - 28.0f - drawH) / 2.0f);
    window.draw(sprite);
}

}  // namespace fireice
