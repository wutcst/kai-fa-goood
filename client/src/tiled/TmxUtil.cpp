#include "TmxUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fireice::tmx {

namespace {

std::optional<std::string> extractJsonStringValue(const std::string& json, const char* key) {
    const std::string keyToken = std::string("\"") + key + "\":";
    const std::size_t start = json.find(keyToken);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    std::size_t valueStart = start + keyToken.size();
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
        ++valueStart;
    }
    if (valueStart >= json.size() || json[valueStart] != '"') {
        return std::nullopt;
    }
    ++valueStart;
    std::string value;
    while (valueStart < json.size()) {
        const char c = json[valueStart];
        if (c == '"') {
            break;
        }
        if (c == '\\' && valueStart + 1 < json.size()) {
            value.push_back(json[valueStart + 1]);
            valueStart += 2;
            continue;
        }
        value.push_back(c);
        ++valueStart;
    }
    return value;
}

std::optional<int> extractJsonIntValue(const std::string& json, const char* key) {
    const std::string keyToken = std::string("\"") + key + "\":";
    const std::size_t start = json.find(keyToken);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    std::size_t valueStart = start + keyToken.size();
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart]))) {
        ++valueStart;
    }
    std::size_t valueEnd = valueStart;
    while (valueEnd < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[valueEnd])) || json[valueEnd] == '-')) {
        ++valueEnd;
    }
    if (valueEnd == valueStart) {
        return std::nullopt;
    }
    return std::stoi(json.substr(valueStart, valueEnd - valueStart));
}

}  // namespace

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<std::string> attributeValue(const std::string& tag, const char* name) {
    const std::string key = std::string(name) + "=\"";
    const std::size_t start = tag.find(key);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t valueStart = start + key.size();
    const std::size_t valueEnd = tag.find('"', valueStart);
    if (valueEnd == std::string::npos) {
        return std::nullopt;
    }
    return tag.substr(valueStart, valueEnd - valueStart);
}

std::string tagName(const std::string& tag) {
    const std::size_t start = tag.find('<');
    if (start == std::string::npos) {
        return {};
    }
    std::size_t end = start + 1;
    while (end < tag.size() && !std::isspace(static_cast<unsigned char>(tag[end])) && tag[end] != '>' &&
           tag[end] != '/') {
        ++end;
    }
    return tag.substr(start + 1, end - start - 1);
}

std::vector<std::string> findTags(const std::string& xml, const char* tagName) {
    std::vector<std::string> tags;
    const std::string open = std::string("<") + tagName;
    const std::size_t nameLen = std::strlen(tagName);
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(open, pos);
        if (pos == std::string::npos) {
            break;
        }
        // Avoid matching longer tag names, e.g. "<imagelayer" when searching for "image".
        const std::size_t afterName = pos + 1 + nameLen;
        if (afterName < xml.size()) {
            const char next = xml[afterName];
            if (std::isalnum(static_cast<unsigned char>(next)) || next == '_' || next == ':') {
                pos += 1;
                continue;
            }
        }
        const std::size_t end = xml.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        tags.push_back(xml.substr(pos, end - pos + 1));
        pos = end + 1;
    }
    return tags;
}

std::optional<std::string> findLayerData(const std::string& xml, const std::string& layerName) {
    const std::string layerOpen = "<layer";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(layerOpen, pos);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        const std::size_t layerEnd = xml.find("</layer>", pos);
        if (layerEnd == std::string::npos) {
            return std::nullopt;
        }
        const std::string layerBlock = xml.substr(pos, layerEnd - pos);
        const std::size_t headerEnd = layerBlock.find('>');
        if (headerEnd == std::string::npos) {
            pos += layerOpen.size();
            continue;
        }
        const std::string header = layerBlock.substr(0, headerEnd + 1);
        const auto name = attributeValue(header, "name");
        if (!name || *name != layerName) {
            pos = layerEnd + 8;
            continue;
        }
        const std::size_t dataStart = layerBlock.find("<data");
        if (dataStart == std::string::npos) {
            return std::nullopt;
        }
        const std::size_t contentStart = layerBlock.find('>', dataStart);
        if (contentStart == std::string::npos) {
            return std::nullopt;
        }
        const std::size_t contentEnd = layerBlock.find("</data>", contentStart);
        if (contentEnd == std::string::npos) {
            return std::nullopt;
        }
        std::string csv = layerBlock.substr(contentStart + 1, contentEnd - contentStart - 1);
        csv.erase(std::remove(csv.begin(), csv.end(), '\n'), csv.end());
        csv.erase(std::remove(csv.begin(), csv.end(), '\r'), csv.end());
        return csv;
    }
}

namespace {

bool attributeIsTrue(const std::string& tag, const char* name) {
    const auto value = attributeValue(tag, name);
    return value && (*value == "1" || *value == "true");
}

std::optional<std::string> extractLayerCsv(const std::string& layerBlock) {
    const std::size_t dataStart = layerBlock.find("<data");
    if (dataStart == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t contentStart = layerBlock.find('>', dataStart);
    if (contentStart == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t contentEnd = layerBlock.find("</data>", contentStart);
    if (contentEnd == std::string::npos) {
        return std::nullopt;
    }
    std::string csv = layerBlock.substr(contentStart + 1, contentEnd - contentStart - 1);
    csv.erase(std::remove(csv.begin(), csv.end(), '\n'), csv.end());
    csv.erase(std::remove(csv.begin(), csv.end(), '\r'), csv.end());
    return csv;
}

float attributeFloat(const std::string& tag, const char* name, float fallback) {
    const auto value = attributeValue(tag, name);
    if (!value) {
        return fallback;
    }
    return std::stof(*value);
}

}  // namespace

std::vector<int> parseCsvInts(const std::string& csv) {
    std::vector<int> values;
    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            continue;
        }
        values.push_back(std::stoi(token));
    }
    return values;
}

std::vector<TileLayerData> findAllTileLayers(const std::string& xml) {
    std::vector<TileLayerData> layers;
    const std::string layerOpen = "<layer";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(layerOpen, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t layerEnd = xml.find("</layer>", pos);
        if (layerEnd == std::string::npos) {
            break;
        }
        const std::string layerBlock = xml.substr(pos, layerEnd - pos);
        const std::size_t headerEnd = layerBlock.find('>');
        if (headerEnd != std::string::npos) {
            const std::string header = layerBlock.substr(0, headerEnd + 1);
            const auto csv = extractLayerCsv(layerBlock);
            if (csv) {
                TileLayerData layer;
                if (const auto name = attributeValue(header, "name")) {
                    layer.name = *name;
                }
                layer.gids = parseCsvInts(*csv);
                layers.push_back(std::move(layer));
            }
        }
        pos = layerEnd + 8;
    }
    return layers;
}

std::optional<std::string> findFirstTileLayerData(const std::string& xml) {
    const auto layers = findAllTileLayers(xml);
    if (layers.empty()) {
        return std::nullopt;
    }
    std::string csv;
    for (int gid : layers.front().gids) {
        if (!csv.empty()) {
            csv += ',';
        }
        csv += std::to_string(gid);
    }
    return csv;
}

std::vector<ImageLayerData> findAllImageLayers(const std::string& xml) {
    std::vector<ImageLayerData> layers;
    const std::string open = "<imagelayer";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(open, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t end = xml.find("</imagelayer>", pos);
        if (end == std::string::npos) {
            break;
        }
        const std::string block = xml.substr(pos, end - pos);
        const std::size_t headerEnd = block.find('>');
        if (headerEnd == std::string::npos) {
            pos = end + 13;
            continue;
        }
        const std::string header = block.substr(0, headerEnd + 1);
        const auto imageTags = findTags(block, "image");
        if (imageTags.empty()) {
            pos = end + 13;
            continue;
        }
        const auto source = attributeValue(imageTags.front(), "source");
        if (!source) {
            pos = end + 13;
            continue;
        }
        ImageLayerData layer;
        layer.imageSource = *source;
        if (const auto width = attributeValue(imageTags.front(), "width")) {
            layer.imageWidth = std::stoi(*width);
        }
        if (const auto height = attributeValue(imageTags.front(), "height")) {
            layer.imageHeight = std::stoi(*height);
        }
        layer.repeatX = attributeIsTrue(header, "repeatx");
        layer.repeatY = attributeIsTrue(header, "repeaty");
        if (const auto offsetX = attributeValue(header, "offsetx")) {
            layer.offsetX = std::stof(*offsetX);
        }
        if (const auto offsetY = attributeValue(header, "offsety")) {
            layer.offsetY = std::stof(*offsetY);
        }
        layers.push_back(std::move(layer));
        pos = end + 13;
    }
    return layers;
}

std::vector<ObjectTileData> findObjectTiles(const std::string& xml) {
    std::vector<ObjectTileData> objects;
    const std::string groupOpen = "<objectgroup";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(groupOpen, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t groupEnd = xml.find("</objectgroup>", pos);
        if (groupEnd == std::string::npos) {
            break;
        }
        const std::string groupBlock = xml.substr(pos, groupEnd - pos);
        for (const std::string& objTag : findTags(groupBlock, "object")) {
            const auto gidText = attributeValue(objTag, "gid");
            if (!gidText) {
                continue;
            }
            ObjectTileData obj;
            obj.gid = std::stoi(*gidText);
            obj.x = attributeFloat(objTag, "x", 0.0f);
            obj.y = attributeFloat(objTag, "y", 0.0f);
            obj.width = attributeFloat(objTag, "width", 0.0f);
            obj.height = attributeFloat(objTag, "height", 0.0f);
            if (const auto name = attributeValue(objTag, "name")) {
                obj.name = *name;
            }
            objects.push_back(std::move(obj));
        }
        pos = groupEnd + 14;
    }
    return objects;
}

std::optional<std::string> jsonStringField(const std::string& json, const char* key) {
    return extractJsonStringValue(json, key);
}

std::optional<int> jsonIntField(const std::string& json, const char* key) {
    return extractJsonIntValue(json, key);
}

std::string parentDirectory(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return ".";
    }
    return path.substr(0, slash);
}

std::string joinPath(const std::string& base, const std::string& relative) {
    if (relative.empty()) {
        return base;
    }
    if (base.empty() || base == ".") {
        return relative;
    }
    const char last = base.back();
    if (last == '/' || last == '\\') {
        return base + relative;
    }
#ifdef _WIN32
    return base + "\\" + relative;
#else
    return base + "/" + relative;
#endif
}

}  // namespace fireice::tmx
