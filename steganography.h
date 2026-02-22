#ifndef STEGANOGRAPHY_H
#define STEGANOGRAPHY_H

#include "image.h"

#include <cstddef>
#include <string>

class Steganography {
public:
    explicit Steganography(RasterImage& image);

    static std::size_t capacityBytes(const RasterImage& image);
    bool encodeMessage(const std::string& message);
    std::string decodeMessage() const;

private:
    RasterImage& m_image;
};

#endif
