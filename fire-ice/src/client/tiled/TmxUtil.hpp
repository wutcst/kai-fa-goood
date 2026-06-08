#pragma once

#include <optional>
#include <string>
#include <vector>

namespace fireice::tmx {

std::string readFile(const std::string& path);
std::optional<std::string> attributeValue(const std::string& tag, const char* name);
std::string tagName(const std::string& tag);
std::vector<std::string> findTags(const std::string& xml, const char* tagName);
std::optional<std::string> findLayerData(const std::string& xml, const std::string& layerName);
std::vector<int> parseCsvInts(const std::string& csv);
std::string parentDirectory(const std::string& path);
std::string joinPath(const std::string& base, const std::string& relative);

} // namespace fireice::tmx
