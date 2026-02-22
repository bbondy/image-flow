#ifndef BMP_H
#define BMP_H

#include "image.h"

#include <string>
#include <vector>

class BMPImage : public RasterImage {
public:
    BMPImage();
    BMPImage(int width, int height, const Color& fill = Color());

    int width() const override;
    int height() const override;

    bool inBounds(int x, int y) const override;
    const Color& getPixel(int x, int y) const override;
    void setPixel(int x, int y, const Color& color) override;

    bool save(const std::string& filename) const;
    static BMPImage load(const std::string& filename);

private:
    int m_width;
    int m_height;
    std::vector<Color> m_pixels;
};

#endif
