#ifndef DRAWABLE_H
#define DRAWABLE_H

#include "image.h"

#include <utility>
#include <vector>

class Drawable {
public:
    enum class LineCap {
        Butt,
        Round,
        Square,
    };

    enum class LineJoin {
        Miter,
        Round,
        Bevel,
    };

    explicit Drawable(Image& image);

    void setPixel(int x, int y, const Color& color);
    const Color& getPixel(int x, int y) const;

    void fill(const Color& color);
    void line(int x0, int y0, int x1, int y1, const Color& color);
    void beginPath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void closePath();
    void setLineWidth(int width);
    void setLineCap(LineCap cap);
    void setLineJoin(LineJoin join);
    void setMiterLimit(float limit);
    void stroke(const Color& color);
    void fillPath(const Color& color);
    void rect(int x, int y, int width, int height, const Color& color);
    void fillRect(int x, int y, int width, int height, const Color& color);
    void roundRect(int x, int y, int width, int height, int radius, const Color& color);
    void fillRoundRect(int x, int y, int width, int height, int radius, const Color& color);
    void ellipse(int cx, int cy, int rx, int ry, const Color& color);
    void fillEllipse(int cx, int cy, int rx, int ry, const Color& color);
    void polyline(const std::vector<std::pair<int, int>>& points, const Color& color);
    void polygon(const std::vector<std::pair<int, int>>& points, const Color& color);
    void fillPolygon(const std::vector<std::pair<int, int>>& points, const Color& color);
    void floodFill(int x, int y, const Color& color, int tolerance = 0);
    void circle(int cx, int cy, int radius, const Color& color);
    void fillCircle(int cx, int cy, int radius, const Color& color);
    void arc(int cx, int cy, int radius, float startRadians, float endRadians, const Color& color, bool counterclockwise = false);

private:
    struct SubPath {
        std::vector<std::pair<float, float>> points;
        bool closed = false;
    };

    Image& m_image;
    std::vector<SubPath> m_path;
    int m_lineWidth = 1;
    LineCap m_lineCap = LineCap::Butt;
    LineJoin m_lineJoin = LineJoin::Miter;
    float m_miterLimit = 10.0f;

    void plotCircleOctants(int cx, int cy, int x, int y, const Color& color);
    void strokeSegment(float x0, float y0, float x1, float y1, const Color& color);
};

#endif
