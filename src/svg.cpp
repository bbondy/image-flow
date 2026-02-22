#include "svg.h"

#include "layer.h"
#include "transform.h"

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

bool parseViewBox(const std::unordered_map<std::string, std::string>& attrs, double& outMinX, double& outMinY, double& outW, double& outH) {
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
    outMinX = values[0];
    outMinY = values[1];
    outW = values[2];
    outH = values[3];
    return true;
}

std::vector<double> parseTransformArgs(const std::string& payload) {
    std::string normalized = payload;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::stringstream ss(normalized);
    std::vector<double> values;
    double v = 0.0;
    while (ss >> v) {
        values.push_back(v);
    }
    return values;
}

Transform2D parseTransform(const std::unordered_map<std::string, std::string>& attrs) {
    auto it = attrs.find("transform");
    if (it == attrs.end()) {
        return Transform2D::identity();
    }

    const std::string& value = it->second;
    std::size_t pos = 0;
    Transform2D total = Transform2D::identity();

    while (pos < value.size()) {
        while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
            ++pos;
        }
        if (pos >= value.size()) {
            break;
        }
        std::size_t nameStart = pos;
        while (pos < value.size() && std::isalpha(static_cast<unsigned char>(value[pos]))) {
            ++pos;
        }
        if (nameStart == pos) {
            break;
        }
        const std::string name = value.substr(nameStart, pos - nameStart);
        while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
            ++pos;
        }
        if (pos >= value.size() || value[pos] != '(') {
            break;
        }
        ++pos;
        std::size_t end = value.find(')', pos);
        if (end == std::string::npos) {
            break;
        }
        const std::string payload = value.substr(pos, end - pos);
        pos = end + 1;

        const std::vector<double> args = parseTransformArgs(payload);
        if (name == "translate" && !args.empty()) {
            const double dx = args[0];
            const double dy = args.size() > 1 ? args[1] : 0.0;
            Transform2D op = Transform2D::translation(dx, dy);
            total = op * total;
        } else if (name == "rotate" && !args.empty()) {
            const double angle = args[0];
            if (args.size() >= 3) {
                Transform2D op = Transform2D::rotationRadians(angle * 3.14159265358979323846 / 180.0, args[1], args[2]);
                total = op * total;
            } else {
                Transform2D op = Transform2D::rotationRadians(angle * 3.14159265358979323846 / 180.0);
                total = op * total;
            }
        } else {
            // Skip unsupported transform types.
        }
    }

    return total;
}

struct PreserveAspectRatio {
    bool valid = false;
    bool none = false;
    bool slice = false;
    double alignX = 0.5;
    double alignY = 0.5;
};

PreserveAspectRatio parsePreserveAspectRatio(const std::unordered_map<std::string, std::string>& attrs) {
    PreserveAspectRatio par;
    auto it = attrs.find("preserveAspectRatio");
    if (it == attrs.end()) {
        par.valid = true;
        return par;
    }

    std::stringstream ss(it->second);
    std::string token;
    if (!(ss >> token)) {
        par.valid = false;
        return par;
    }
    if (token == "none") {
        par.valid = true;
        par.none = true;
        return par;
    }

    if (token == "xMinYMin") {
        par.alignX = 0.0;
        par.alignY = 0.0;
    } else if (token == "xMidYMin") {
        par.alignX = 0.5;
        par.alignY = 0.0;
    } else if (token == "xMaxYMin") {
        par.alignX = 1.0;
        par.alignY = 0.0;
    } else if (token == "xMinYMid") {
        par.alignX = 0.0;
        par.alignY = 0.5;
    } else if (token == "xMidYMid") {
        par.alignX = 0.5;
        par.alignY = 0.5;
    } else if (token == "xMaxYMid") {
        par.alignX = 1.0;
        par.alignY = 0.5;
    } else if (token == "xMinYMax") {
        par.alignX = 0.0;
        par.alignY = 1.0;
    } else if (token == "xMidYMax") {
        par.alignX = 0.5;
        par.alignY = 1.0;
    } else if (token == "xMaxYMax") {
        par.alignX = 1.0;
        par.alignY = 1.0;
    } else {
        par.valid = false;
        return par;
    }

    std::string mode;
    if (ss >> mode) {
        par.slice = (mode == "slice");
    }
    par.valid = true;
    return par;
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

namespace {
SVGImage loadSVGImpl(const std::string& filename, int forcedWidth, int forcedHeight, bool useForcedSize) {
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
    double viewMinX = 0.0;
    double viewMinY = 0.0;
    double viewW = 0.0;
    double viewH = 0.0;
    const bool hasViewBox = parseViewBox(root.attrs, viewMinX, viewMinY, viewW, viewH);
    if (useForcedSize) {
        if (forcedWidth <= 0 || forcedHeight <= 0) {
            throw std::runtime_error("Invalid forced raster size");
        }
        width = forcedWidth;
        height = forcedHeight;
    } else {
        const bool hasWidth = parseIntAttr(root.attrs, "width", width);
        const bool hasHeight = parseIntAttr(root.attrs, "height", height);
        if (!hasWidth || !hasHeight) {
            if (hasViewBox) {
                if (!hasWidth) {
                    width = static_cast<int>(std::lround(viewW));
                }
                if (!hasHeight) {
                    height = static_cast<int>(std::lround(viewH));
                }
            }
        }
        if (width <= 0 || height <= 0) {
            throw std::runtime_error("Invalid SVG dimensions (missing width/height or viewBox)");
        }
    }

    SVGImage image(width, height, Color(255, 255, 255));

    PreserveAspectRatio par = parsePreserveAspectRatio(root.attrs);
    if (hasViewBox && !par.valid) {
        throw std::runtime_error("Invalid preserveAspectRatio");
    }
    if (!hasViewBox) {
        par.none = true;
    }

    double scaleX = 1.0;
    double scaleY = 1.0;
    double alignOffsetX = 0.0;
    double alignOffsetY = 0.0;

    if (hasViewBox) {
        const double sx = static_cast<double>(width) / viewW;
        const double sy = static_cast<double>(height) / viewH;
        if (par.none) {
            scaleX = sx;
            scaleY = sy;
        } else {
            const double scale = par.slice ? std::max(sx, sy) : std::min(sx, sy);
            scaleX = scale;
            scaleY = scale;
            const double contentW = viewW * scale;
            const double contentH = viewH * scale;
            alignOffsetX = (static_cast<double>(width) - contentW) * par.alignX;
            alignOffsetY = (static_cast<double>(height) - contentH) * par.alignY;
        }
    }

    Transform2D viewTransform = Transform2D::identity();
    if (hasViewBox) {
        viewTransform = Transform2D::translation(-viewMinX, -viewMinY);
        Transform2D scale = Transform2D::fromMatrix(scaleX, 0.0, 0.0, scaleY, alignOffsetX, alignOffsetY);
        viewTransform = scale * viewTransform;
    }

    std::function<void(const XmlNode&, const Transform2D&)> visit = [&](const XmlNode& node, const Transform2D& parentTransform) {
        const Transform2D localTransform = parseTransform(node.attrs);
        const Transform2D combinedTransform = parentTransform * localTransform;
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

            if (hasW && hasH && hasFill) {
                const double x0 = static_cast<double>(hasX ? rectX : 0);
                const double y0 = static_cast<double>(hasY ? rectY : 0);
                const double x1 = x0 + static_cast<double>(rectW);
                const double y1 = y0 + static_cast<double>(rectH);

                const Transform2D totalTransform = viewTransform * combinedTransform;

                const auto c1 = totalTransform.apply(x0, y0);
                const auto c2 = totalTransform.apply(x1, y0);
                const auto c3 = totalTransform.apply(x0, y1);
                const auto c4 = totalTransform.apply(x1, y1);

                double minX = std::min(std::min(c1.first, c2.first), std::min(c3.first, c4.first));
                double minY = std::min(std::min(c1.second, c2.second), std::min(c3.second, c4.second));
                double maxX = std::max(std::max(c1.first, c2.first), std::max(c3.first, c4.first));
                double maxY = std::max(std::max(c1.second, c2.second), std::max(c3.second, c4.second));

                int startX = std::max(0, static_cast<int>(std::floor(minX)));
                int startY = std::max(0, static_cast<int>(std::floor(minY)));
                int endX = std::min(width, static_cast<int>(std::ceil(maxX)));
                int endY = std::min(height, static_cast<int>(std::ceil(maxY)));

                if (startX == 0 && startY == 0 && endX == width && endY == height && combinedTransform.isIdentity()) {
                    for (int py = 0; py < height; ++py) {
                        for (int px = 0; px < width; ++px) {
                            image.setPixel(px, py, fill);
                        }
                    }
                } else {
                    for (int py = startY; py < endY; ++py) {
                        for (int px = startX; px < endX; ++px) {
                            const auto local = totalTransform.applyInverse(static_cast<double>(px) + 0.5, static_cast<double>(py) + 0.5);
                            if (local.first >= x0 && local.first < x1 && local.second >= y0 && local.second < y1) {
                                image.setPixel(px, py, fill);
                            }
                        }
                    }
                }
            }
        }

        for (const XmlNode& child : node.children) {
            visit(child, combinedTransform);
        }
    };

    visit(root, Transform2D::identity());

    return image;
}
} // namespace

SVGImage SVGImage::load(const std::string& filename) {
    return loadSVGImpl(filename, 0, 0, false);
}

SVGImage SVGImage::load(const std::string& filename, int rasterWidth, int rasterHeight) {
    return loadSVGImpl(filename, rasterWidth, rasterHeight, true);
}

void copyToRasterImage(const SVGImage& source, RasterImage& destination) {
    if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::invalid_argument("copyToRasterImage(SVGImage, RasterImage) dimensions must match");
    }

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            destination.setPixel(x, y, source.getPixel(x, y));
        }
    }
}

void copyToLayer(const SVGImage& source, Layer& destination, std::uint8_t alpha) {
    if (source.width() != destination.image().width() || source.height() != destination.image().height()) {
        throw std::invalid_argument("copyToLayer(SVGImage, Layer) dimensions must match");
    }

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const Color& p = source.getPixel(x, y);
            destination.image().setPixel(x, y, PixelRGBA8(p.r, p.g, p.b, alpha));
        }
    }
}

void rasterizeSVGFileToRaster(const std::string& filename, RasterImage& destination) {
    const SVGImage source = SVGImage::load(filename, destination.width(), destination.height());
    copyToRasterImage(source, destination);
}

void rasterizeSVGFileToLayer(const std::string& filename, Layer& destination, std::uint8_t alpha) {
    const SVGImage source = SVGImage::load(filename, destination.image().width(), destination.image().height());
    copyToLayer(source, destination, alpha);
}
