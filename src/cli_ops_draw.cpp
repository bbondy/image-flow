#include "cli_ops_draw.h"
#include "cli_ops_resolve.h"

#include "cli_parse.h"
#include "drawable.h"

#include <cmath>
#include <stdexcept>

namespace {
class BufferImageView final : public Image {
public:
    explicit BufferImageView(ImageBuffer& buffer, std::uint8_t drawAlpha = 255, bool forceAlpha = true)
        : m_buffer(buffer), m_lastColor(0, 0, 0), m_drawAlpha(drawAlpha), m_forceAlpha(forceAlpha) {}

    int width() const override { return m_buffer.width(); }
    int height() const override { return m_buffer.height(); }
    bool inBounds(int x, int y) const override { return m_buffer.inBounds(x, y); }

    const Color& getPixel(int x, int y) const override {
        if (!m_buffer.inBounds(x, y)) {
            m_lastColor = Color(0, 0, 0);
            return m_lastColor;
        }
        const PixelRGBA8& px = m_buffer.getPixel(x, y);
        m_lastColor = Color(px.r, px.g, px.b);
        return m_lastColor;
    }

    void setPixel(int x, int y, const Color& color) override {
        if (!m_buffer.inBounds(x, y)) {
            return;
        }
        const PixelRGBA8 src = m_buffer.getPixel(x, y);
        const std::uint8_t alpha = m_forceAlpha ? m_drawAlpha : src.a;
        m_buffer.setPixel(x, y, PixelRGBA8(color.r, color.g, color.b, alpha));
    }

private:
    ImageBuffer& m_buffer;
    mutable Color m_lastColor;
    std::uint8_t m_drawAlpha;
    bool m_forceAlpha;
};
} // namespace

bool tryApplyDrawOperation(
    const std::string& action,
    Document& document,
    const std::unordered_map<std::string, std::string>& kv) {
    if (action == "draw-fill") {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fill(Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-line") {
        if (kv.find("path") == kv.end() || kv.find("x0") == kv.end() || kv.find("y0") == kv.end() ||
            kv.find("x1") == kv.end() || kv.find("y1") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-line requires path= x0= y0= x1= y1= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.line(std::stoi(kv.at("x0")), std::stoi(kv.at("y0")),
                      std::stoi(kv.at("x1")), std::stoi(kv.at("y1")),
                      Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-rect") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-rect requires path= x= y= width= height= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.rect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                      std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                      Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-fill-rect") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-rect requires path= x= y= width= height= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillRect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                          std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                          Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-round-rect") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-round-rect requires path= x= y= width= height= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.roundRect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                           std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                           std::stoi(kv.at("radius")), Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-fill-round-rect") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("width") == kv.end() || kv.find("height") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-round-rect requires path= x= y= width= height= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillRoundRect(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                               std::stoi(kv.at("width")), std::stoi(kv.at("height")),
                               std::stoi(kv.at("radius")), Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-ellipse") {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("rx") == kv.end() || kv.find("ry") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-ellipse requires path= cx= cy= rx= ry= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.ellipse(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")),
                         std::stoi(kv.at("rx")), std::stoi(kv.at("ry")),
                         Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-fill-ellipse") {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("rx") == kv.end() || kv.find("ry") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-ellipse requires path= cx= cy= rx= ry= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillEllipse(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")),
                             std::stoi(kv.at("rx")), std::stoi(kv.at("ry")),
                             Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-polyline") {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-polyline requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 2, "draw-polyline");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.polyline(points, Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-polygon") {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-polygon requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 3, "draw-polygon");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.polygon(points, Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-fill-polygon") {
        if (kv.find("path") == kv.end() || kv.find("points") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-polygon requires path= points= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const std::vector<std::pair<int, int>> points = parseDrawPoints(kv.at("points"), 3, "draw-fill-polygon");
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillPolygon(points, Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-flood-fill") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() ||
            kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-flood-fill requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const int tolerance = kv.find("tolerance") == kv.end() ? 0 : std::stoi(kv.at("tolerance"));
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.floodFill(std::stoi(kv.at("x")), std::stoi(kv.at("y")),
                           Color(rgba.r, rgba.g, rgba.b), tolerance);
        return true;
    }

    if (action == "draw-circle") {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-circle requires path= cx= cy= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.circle(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                        Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-fill-circle") {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-fill-circle requires path= cx= cy= radius= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.fillCircle(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                            Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-arc") {
        if (kv.find("path") == kv.end() || kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("radius") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-arc requires path= cx= cy= radius= rgba= and start/end");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);

        float startRadians = 0.0f;
        float endRadians = 0.0f;
        if (kv.find("start_rad") != kv.end() && kv.find("end_rad") != kv.end()) {
            startRadians = std::stof(kv.at("start_rad"));
            endRadians = std::stof(kv.at("end_rad"));
        } else if (kv.find("start_deg") != kv.end() && kv.find("end_deg") != kv.end()) {
            startRadians = static_cast<float>(std::stod(kv.at("start_deg")) * 3.14159265358979323846 / 180.0);
            endRadians = static_cast<float>(std::stod(kv.at("end_deg")) * 3.14159265358979323846 / 180.0);
        } else {
            throw std::runtime_error("draw-arc requires start_rad/end_rad or start_deg/end_deg");
        }
        const bool counterclockwise = kv.find("counterclockwise") == kv.end() ? false : parseBoolFlag(kv.at("counterclockwise"));

        drawable.arc(std::stoi(kv.at("cx")), std::stoi(kv.at("cy")), std::stoi(kv.at("radius")),
                     startRadians, endRadians, Color(rgba.r, rgba.g, rgba.b), counterclockwise);
        return true;
    }

    if (action == "draw-quadratic-bezier") {
        if (kv.find("path") == kv.end() || kv.find("x0") == kv.end() || kv.find("y0") == kv.end() ||
            kv.find("cx") == kv.end() || kv.find("cy") == kv.end() ||
            kv.find("x1") == kv.end() || kv.find("y1") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-quadratic-bezier requires path= x0= y0= cx= cy= x1= y1= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.beginPath();
        drawable.moveTo(std::stof(kv.at("x0")), std::stof(kv.at("y0")));
        drawable.quadraticCurveTo(std::stof(kv.at("cx")), std::stof(kv.at("cy")),
                                  std::stof(kv.at("x1")), std::stof(kv.at("y1")));
        drawable.stroke(Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    if (action == "draw-bezier") {
        if (kv.find("path") == kv.end() || kv.find("x0") == kv.end() || kv.find("y0") == kv.end() ||
            kv.find("cx1") == kv.end() || kv.find("cy1") == kv.end() ||
            kv.find("cx2") == kv.end() || kv.find("cy2") == kv.end() ||
            kv.find("x1") == kv.end() || kv.find("y1") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("draw-bezier requires path= x0= y0= cx1= cy1= cx2= cy2= x1= y1= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        ImageBuffer& targetBuffer = resolveDrawTargetBuffer(layer, kv);
        const PixelRGBA8 rgba = parseRGBA(kv.at("rgba"), true);
        BufferImageView view(targetBuffer, rgba.a, true);
        Drawable drawable(view);
        drawable.beginPath();
        drawable.moveTo(std::stof(kv.at("x0")), std::stof(kv.at("y0")));
        drawable.bezierCurveTo(std::stof(kv.at("cx1")), std::stof(kv.at("cy1")),
                               std::stof(kv.at("cx2")), std::stof(kv.at("cy2")),
                               std::stof(kv.at("x1")), std::stof(kv.at("y1")));
        drawable.stroke(Color(rgba.r, rgba.g, rgba.b));
        return true;
    }

    return false;
}
