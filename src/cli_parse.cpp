#include "cli_parse.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
} // namespace

std::vector<std::string> tokenizeOpSpec(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    char quote = '\0';
    bool escaping = false;

    for (char ch : text) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            } else {
                current.push_back(ch);
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (escaping) {
        throw std::runtime_error("Invalid op: trailing escape character");
    }
    if (quote != '\0') {
        throw std::runtime_error("Invalid op: unterminated quoted value");
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::vector<std::string> splitByChar(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream in(text);
    while (std::getline(in, current, delimiter)) {
        parts.push_back(current);
    }
    return parts;
}

std::vector<std::string> splitNonEmptyByChar(const std::string& text, char delimiter) {
    const std::vector<std::string> all = splitByChar(text, delimiter);
    std::vector<std::string> out;
    for (const std::string& value : all) {
        if (!value.empty()) {
            out.push_back(value);
        }
    }
    return out;
}

int parseIntStrict(const std::string& text, const std::string& fieldName) {
    std::size_t parsed = 0;
    const long long value = std::stoll(text, &parsed, 10);
    if (parsed != text.size()) {
        throw std::runtime_error("Invalid integer for " + fieldName + ": " + text);
    }
    if (value < static_cast<long long>(std::numeric_limits<int>::min()) ||
        value > static_cast<long long>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Integer out of range for " + fieldName + ": " + text);
    }
    return static_cast<int>(value);
}

int parseIntInRange(const std::string& text, const std::string& fieldName, int minValue, int maxValue) {
    const int value = parseIntStrict(text, fieldName);
    if (value < minValue || value > maxValue) {
        throw std::runtime_error("Value out of range for " + fieldName + ": " + text +
                                 " (expected " + std::to_string(minValue) + ".." + std::to_string(maxValue) + ")");
    }
    return value;
}

double parseDoubleStrict(const std::string& text, const std::string& fieldName) {
    std::size_t parsed = 0;
    const double value = std::stod(text, &parsed);
    if (parsed != text.size()) {
        throw std::runtime_error("Invalid number for " + fieldName + ": " + text);
    }
    return value;
}

std::uint8_t parseByte(const std::string& text, const std::string& fieldName) {
    return static_cast<std::uint8_t>(parseIntInRange(text, fieldName, 0, 255));
}

bool parseBoolFlag(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    throw std::runtime_error("Invalid boolean value: " + value);
}

std::pair<int, int> parseIntPair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected integer pair x,y but got: " + text);
    }
    return {parseIntStrict(parts[0], "x"), parseIntStrict(parts[1], "y")};
}

std::pair<double, double> parseDoublePair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected numeric pair x,y but got: " + text);
    }
    return {parseDoubleStrict(parts[0], "x"), parseDoubleStrict(parts[1], "y")};
}

std::vector<std::pair<int, int>> parseDrawPoints(const std::string& text,
                                                 std::size_t minPoints,
                                                 const std::string& action) {
    const std::vector<std::string> tokens = splitNonEmptyByChar(text, ';');
    std::vector<std::pair<int, int>> points;
    points.reserve(tokens.size());
    for (const std::string& token : tokens) {
        points.push_back(parseIntPair(token));
    }
    if (points.size() < minPoints) {
        throw std::runtime_error(action + " requires at least " + std::to_string(minPoints) +
                                 " points in points=x0,y0;x1,y1;...");
    }
    return points;
}

PixelRGBA8 parseRGBA(const std::string& text, bool allowRgb) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() == 3 && allowRgb) {
        return PixelRGBA8(parseByte(parts[0], "r"),
                          parseByte(parts[1], "g"),
                          parseByte(parts[2], "b"),
                          255);
    }
    if (parts.size() != 4) {
        throw std::runtime_error("Expected rgba=r,g,b,a but got: " + text);
    }
    return PixelRGBA8(parseByte(parts[0], "r"),
                      parseByte(parts[1], "g"),
                      parseByte(parts[2], "b"),
                      parseByte(parts[3], "a"));
}
