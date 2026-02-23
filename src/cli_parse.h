#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include "layer.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

std::vector<std::string> tokenizeOpSpec(const std::string& text);
std::vector<std::string> splitByChar(const std::string& text, char delimiter);
std::vector<std::string> splitNonEmptyByChar(const std::string& text, char delimiter);
int parseIntStrict(const std::string& text, const std::string& fieldName);
int parseIntInRange(const std::string& text, const std::string& fieldName, int minValue, int maxValue);
double parseDoubleStrict(const std::string& text, const std::string& fieldName);
std::uint8_t parseByte(const std::string& text, const std::string& fieldName);
bool parseBoolFlag(const std::string& value);
std::pair<int, int> parseIntPair(const std::string& text);
std::pair<double, double> parseDoublePair(const std::string& text);
std::vector<std::pair<int, int>> parseDrawPoints(const std::string& text, std::size_t minPoints, const std::string& action);
PixelRGBA8 parseRGBA(const std::string& text, bool allowRgb = false);

#endif
