#pragma once

#include <optional>
#include <string>
#include <vector>

namespace fireice::tmx {

struct TileLayerData {
    std::string name;
    std::vector<int> gids;
};

struct ImageLayerData {
    std::string imageSource;
    int imageWidth = 0;
    int imageHeight = 0;
    bool repeatX = false;
    bool repeatY = false;
};

struct ObjectTileData {
    int gid = 0;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::string name;
};

std::string readFile(const std::string& path);
std::optional<std::string> attributeValue(const std::string& tag, const char* name);
std::string tagName(const std::string& tag);
std::vector<std::string> findTags(const std::string& xml, const char* tagName);
std::optional<std::string> findLayerData(const std::string& xml, const std::string& layerName);
std::optional<std::string> findFirstTileLayerData(const std::string& xml);
std::vector<TileLayerData> findAllTileLayers(const std::string& xml);
std::vector<ImageLayerData> findAllImageLayers(const std::string& xml);
std::vector<ObjectTileData> findObjectTiles(const std::string& xml);
std::optional<std::string> jsonStringField(const std::string& json, const char* key);
std::optional<int> jsonIntField(const std::string& json, const char* key);
std::vector<int> parseCsvInts(const std::string& csv);
std::string parentDirectory(const std::string& path);
std::string joinPath(const std::string& base, const std::string& relative);

}  // namespace fireice::tmx
