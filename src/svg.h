#ifndef SVG_H
#define SVG_H

#include "image.h"

#include <cstdint>
#include <string>
#include <vector>

class Layer;

class SVGImage : public VectorImage {
public:
    SVGImage();
    SVGImage(int width, int height, const Color& fill = Color());

    int width() const override;
    int height() const override;
    bool inBounds(int x, int y) const override;
    const Color& getPixel(int x, int y) const override;
    void setPixel(int x, int y, const Color& color) override;

    bool save(const std::string& filename) const;
    static SVGImage load(const std::string& filename);
    static SVGImage load(const std::string& filename, int rasterWidth, int rasterHeight);

private:
    int m_width;
    int m_height;
    std::vector<Color> m_pixels;
};

void copyToRasterImage(const SVGImage& source, RasterImage& destination);
void copyToLayer(const SVGImage& source, Layer& destination, std::uint8_t alpha = 255);
void rasterizeSVGFileToRaster(const std::string& filename, RasterImage& destination);
void rasterizeSVGFileToLayer(const std::string& filename, Layer& destination, std::uint8_t alpha = 255);

#endif
