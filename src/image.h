#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;

    Color() : r(0), g(0), b(0) {}
    Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
        : r(red), g(green), b(blue) {}
};

class Image {
public:
    virtual ~Image() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool inBounds(int x, int y) const = 0;
    virtual const Color& getPixel(int x, int y) const = 0;
    virtual void setPixel(int x, int y, const Color& color) = 0;
};

class RasterImage : public Image {
public:
    ~RasterImage() override = default;
};

class VectorImage : public Image {
public:
    ~VectorImage() override = default;
};

#endif
