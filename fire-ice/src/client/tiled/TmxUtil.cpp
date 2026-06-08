#include "TmxUtil.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace fireice::tmx {

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
    while (end < tag.size() && !std::isspace(static_cast<unsigned char>(tag[end])) && tag[end] != '>' && tag[end] != '/') {
        ++end;
    }
    return tag.substr(start + 1, end - start - 1);
}

std::vector<std::string> findTags(const std::string& xml, const char* tagName) {
    std::vector<std::string> tags;
    const std::string open = std::string("<") + tagName;
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(open, pos);
        if (pos == std::string::npos) {
            break;
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

} // namespace fireice::tmx
