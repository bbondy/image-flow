#ifndef DRAWABLE_H
#define DRAWABLE_H

#include "image.h"

class Drawable {
public:
    explicit Drawable(Image& image);

    void setPixel(int x, int y, const Color& color);
    const Color& getPixel(int x, int y) const;

    void fill(const Color& color);
    void line(int x0, int y0, int x1, int y1, const Color& color);
    void circle(int cx, int cy, int radius, const Color& color);
    void fillCircle(int cx, int cy, int radius, const Color& color);
    void arc(int cx, int cy, int radius, float startRadians, float endRadians, const Color& color);

private:
    Image& m_image;

    void plotCircleOctants(int cx, int cy, int x, int y, const Color& color);
};

#endif
