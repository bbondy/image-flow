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
    void stroke(const Color& color);
    void fillPath(const Color& color);
    void rect(int x, int y, int width, int height, const Color& color);
    void fillRect(int x, int y, int width, int height, const Color& color);
    void ellipse(int cx, int cy, int rx, int ry, const Color& color);
    void fillEllipse(int cx, int cy, int rx, int ry, const Color& color);
    void polyline(const std::vector<std::pair<int, int>>& points, const Color& color);
    void polygon(const std::vector<std::pair<int, int>>& points, const Color& color);
    void fillPolygon(const std::vector<std::pair<int, int>>& points, const Color& color);
    void circle(int cx, int cy, int radius, const Color& color);
    void fillCircle(int cx, int cy, int radius, const Color& color);
    void arc(int cx, int cy, int radius, float startRadians, float endRadians, const Color& color);

private:
    struct SubPath {
        std::vector<std::pair<float, float>> points;
        bool closed = false;
    };

    Image& m_image;
    std::vector<SubPath> m_path;

    void plotCircleOctants(int cx, int cy, int x, int y, const Color& color);
    void strokeSegment(float x0, float y0, float x1, float y1, const Color& color);
};

#endif
