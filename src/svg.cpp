#include "svg.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {
std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0;
}

struct XmlNode {
    std::string name;
    std::unordered_map<std::string, std::string> attrs;
    std::vector<XmlNode> children;
};

class XmlParser {
public:
    explicit XmlParser(const std::string& text) : m_text(text), m_pos(0) {}

    XmlNode parse() {
        skipProlog();
        skipWhitespace();
        XmlNode root = parseElement();
        return root;
    }

private:
    const std::string& m_text;
    std::size_t m_pos;

    void skipWhitespace() {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
            ++m_pos;
        }
    }

    bool startsWith(const std::string& token) const {
        return m_text.compare(m_pos, token.size(), token) == 0;
    }

    void skipProlog() {
        skipWhitespace();
        if (startsWith("<?xml")) {
            std::size_t end = m_text.find("?>", m_pos);
            if (end == std::string::npos) {
                throw std::runtime_error("Malformed XML prolog");
            }
            m_pos = end + 2;
        }
        while (true) {
            skipWhitespace();
            if (startsWith("<!--")) {
                std::size_t end = m_text.find("-->", m_pos);
                if (end == std::string::npos) {
                    throw std::runtime_error("Unterminated XML comment");
                }
                m_pos = end + 3;
                continue;
            }
            break;
        }
    }

    XmlNode parseElement() {
        expect('<');
        if (m_pos < m_text.size() && m_text[m_pos] == '/') {
            throw std::runtime_error("Unexpected closing tag");
        }

        XmlNode node;
        node.name = parseName();
        skipWhitespace();
        while (m_pos < m_text.size() && m_text[m_pos] != '>' && m_text[m_pos] != '/') {
            const std::string key = parseName();
            skipWhitespace();
            expect('=');
            skipWhitespace();
            const std::string value = parseQuoted();
            node.attrs[key] = value;
            skipWhitespace();
        }

        if (m_pos < m_text.size() && m_text[m_pos] == '/') {
            ++m_pos;
            expect('>');
            return node;
        }

        expect('>');

        while (true) {
            skipWhitespace();
            if (startsWith("<!--")) {
                std::size_t end = m_text.find("-->", m_pos);
                if (end == std::string::npos) {
                    throw std::runtime_error("Unterminated XML comment");
                }
                m_pos = end + 3;
                continue;
            }
            if (startsWith("</")) {
                m_pos += 2;
                const std::string closeName = parseName();
                skipWhitespace();
                expect('>');
                if (closeName != node.name) {
                    throw std::runtime_error("Mismatched XML closing tag");
                }
                break;
            }
            if (m_pos >= m_text.size()) {
                throw std::runtime_error("Unexpected end of XML");
            }
            if (m_text[m_pos] == '<') {
                node.children.push_back(parseElement());
            } else {
                skipText();
            }
        }

        return node;
    }

    void skipText() {
        while (m_pos < m_text.size() && m_text[m_pos] != '<') {
            ++m_pos;
        }
    }

    void expect(char c) {
        if (m_pos >= m_text.size() || m_text[m_pos] != c) {
            throw std::runtime_error("Malformed XML");
        }
        ++m_pos;
    }

    std::string parseName() {
        if (m_pos >= m_text.size() || !(std::isalpha(static_cast<unsigned char>(m_text[m_pos])) || m_text[m_pos] == '_')) {
            throw std::runtime_error("Expected XML name");
        }
        std::size_t start = m_pos++;
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':') {
                ++m_pos;
            } else {
                break;
            }
        }
        return m_text.substr(start, m_pos - start);
    }

    std::string parseQuoted() {
        if (m_pos >= m_text.size() || (m_text[m_pos] != '"' && m_text[m_pos] != '\'')) {
            throw std::runtime_error("Expected quoted XML attribute");
        }
        const char quote = m_text[m_pos++];
        std::size_t start = m_pos;
        while (m_pos < m_text.size() && m_text[m_pos] != quote) {
            ++m_pos;
        }
        if (m_pos >= m_text.size()) {
            throw std::runtime_error("Unterminated XML attribute");
        }
        std::string value = m_text.substr(start, m_pos - start);
        ++m_pos;
        return value;
    }
};

bool parseIntAttr(const std::unordered_map<std::string, std::string>& attrs, const std::string& name, int& out) {
    auto it = attrs.find(name);
    if (it == attrs.end()) {
        return false;
    }
    const std::string& value = it->second;
    if (value.empty()) {
        return false;
    }
    std::size_t i = 0;
    while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) {
        ++i;
    }
    if (i == 0) {
        return false;
    }
    out = std::stoi(value.substr(0, i));
    return true;
}

bool parseViewBox(const std::unordered_map<std::string, std::string>& attrs, int& outW, int& outH) {
    auto it = attrs.find("viewBox");
    if (it == attrs.end()) {
        return false;
    }
    std::stringstream ss(it->second);
    double values[4] = {0.0, 0.0, 0.0, 0.0};
    int idx = 0;
    while (idx < 4) {
        if (!(ss >> values[idx])) {
            return false;
        }
        ++idx;
    }
    if (values[2] <= 0.0 || values[3] <= 0.0) {
        return false;
    }
    outW = static_cast<int>(std::lround(values[2]));
    outH = static_cast<int>(std::lround(values[3]));
    return outW > 0 && outH > 0;
}

bool parseTranslate(const std::unordered_map<std::string, std::string>& attrs, int& outDx, int& outDy) {
    auto it = attrs.find("transform");
    if (it == attrs.end()) {
        return false;
    }
    const std::string& value = it->second;
    const std::string key = "translate(";
    std::size_t pos = value.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos += key.size();
    std::size_t end = value.find(')', pos);
    if (end == std::string::npos) {
        return false;
    }
    std::string payload = value.substr(pos, end - pos);
    std::replace(payload.begin(), payload.end(), ',', ' ');
    std::stringstream ss(payload);
    double dx = 0.0;
    double dy = 0.0;
    if (!(ss >> dx)) {
        return false;
    }
    if (!(ss >> dy)) {
        dy = 0.0;
    }
    outDx = static_cast<int>(std::lround(dx));
    outDy = static_cast<int>(std::lround(dy));
    return true;
}

bool parseColorAttr(const std::unordered_map<std::string, std::string>& attrs, Color& out) {
    auto it = attrs.find("fill");
    if (it == attrs.end()) {
        return false;
    }
    const std::string& value = it->second;
    if (value.compare(0, 4, "rgb(") == 0) {
        std::size_t start = 4;
        std::size_t end = value.find(')', start);
        if (end == std::string::npos) {
            return false;
        }
        const std::string payload = value.substr(start, end - start);
        std::stringstream ss(payload);
        std::string token;
        int values[3] = {0, 0, 0};
        int idx = 0;
        while (std::getline(ss, token, ',') && idx < 3) {
            values[idx++] = std::stoi(token);
        }
        if (idx != 3) {
            return false;
        }
        out = Color(static_cast<std::uint8_t>(values[0]),
                    static_cast<std::uint8_t>(values[1]),
                    static_cast<std::uint8_t>(values[2]));
        return true;
    }
    if (!value.empty() && value[0] == '#' && value.size() >= 7) {
        const int r = (hexValue(value[1]) << 4) + hexValue(value[2]);
        const int g = (hexValue(value[3]) << 4) + hexValue(value[4]);
        const int b = (hexValue(value[5]) << 4) + hexValue(value[6]);
        out = Color(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b));
        return true;
    }
    return false;
}
} // namespace

SVGImage::SVGImage() : m_width(0), m_height(0) {}

SVGImage::SVGImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int SVGImage::width() const {
    return m_width;
}

int SVGImage::height() const {
    return m_height;
}

bool SVGImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& SVGImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void SVGImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool SVGImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    std::ofstream out(filename);
    if (!out) {
        return false;
    }

    const Color background = m_pixels.empty() ? Color(255, 255, 255) : m_pixels.front();

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << m_width
        << "\" height=\"" << m_height << "\" viewBox=\"0 0 " << m_width << " " << m_height << "\">\n";
    out << "  <rect width=\"" << m_width << "\" height=\"" << m_height
        << "\" fill=\"rgb(" << static_cast<int>(background.r) << "," << static_cast<int>(background.g) << ","
        << static_cast<int>(background.b) << ")\"/>\n";

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const Color& px = m_pixels[pixelIndex(x, y, m_width)];
            if (px.r == background.r && px.g == background.g && px.b == background.b) {
                continue;
            }
            out << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"1\" height=\"1\" fill=\"rgb("
                << static_cast<int>(px.r) << "," << static_cast<int>(px.g) << "," << static_cast<int>(px.b)
                << ")\"/>\n";
        }
    }

    out << "</svg>\n";
    return static_cast<bool>(out);
}

SVGImage SVGImage::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("Cannot open SVG file: " + filename);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string content = buffer.str();

    XmlParser parser(content);
    XmlNode root = parser.parse();
    if (root.name != "svg") {
        throw std::runtime_error("Root element is not svg");
    }
    int width = 0;
    int height = 0;
    const bool hasWidth = parseIntAttr(root.attrs, "width", width);
    const bool hasHeight = parseIntAttr(root.attrs, "height", height);
    if (!hasWidth || !hasHeight) {
        int viewW = 0;
        int viewH = 0;
        if (parseViewBox(root.attrs, viewW, viewH)) {
            if (!hasWidth) {
                width = viewW;
            }
            if (!hasHeight) {
                height = viewH;
            }
        }
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid SVG dimensions (missing width/height or viewBox)");
    }

    SVGImage image(width, height, Color(255, 255, 255));

    std::function<void(const XmlNode&, int, int)> visit = [&](const XmlNode& node, int offsetX, int offsetY) {
        int localDx = 0;
        int localDy = 0;
        if (parseTranslate(node.attrs, localDx, localDy)) {
            offsetX += localDx;
            offsetY += localDy;
        }
        if (node.name == "rect") {
            int rectW = 0;
            int rectH = 0;
            int rectX = 0;
            int rectY = 0;
            const bool hasW = parseIntAttr(node.attrs, "width", rectW);
            const bool hasH = parseIntAttr(node.attrs, "height", rectH);
            const bool hasX = parseIntAttr(node.attrs, "x", rectX);
            const bool hasY = parseIntAttr(node.attrs, "y", rectY);
            Color fill;
            const bool hasFill = parseColorAttr(node.attrs, fill);

            const int finalX = (hasX ? rectX : 0) + offsetX;
            const int finalY = (hasY ? rectY : 0) + offsetY;

            if (hasW && hasH && rectW == width && rectH == height && hasFill && finalX == 0 && finalY == 0) {
                std::fill(image.m_pixels.begin(), image.m_pixels.end(), fill);
            } else if (hasW && hasH && hasFill && rectW == 1 && rectH == 1) {
                image.setPixel(finalX, finalY, fill);
            }
        }

        for (const XmlNode& child : node.children) {
            visit(child, offsetX, offsetY);
        }
    };

    visit(root, 0, 0);

    return image;
}
