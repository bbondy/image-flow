#ifndef GIF_H
#define GIF_H

#include "image.h"

#include <string>
#include <vector>

class GIFImage : public RasterImage {
public:
    GIFImage();
    GIFImage(int width, int height, const Color& fill = Color());

    int width() const override;
    int height() const override;

    bool inBounds(int x, int y) const override;
    const Color& getPixel(int x, int y) const override;
    void setPixel(int x, int y, const Color& color) override;

    bool save(const std::string& filename) const;
    static GIFImage load(const std::string& filename);

private:
    int m_width;
    int m_height;
    std::vector<Color> m_pixels;
};

#endif
