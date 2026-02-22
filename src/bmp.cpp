#include "bmp.h"

#include <fstream>
#include <stdexcept>

namespace {
#pragma pack(push, 1)
struct BMPFileHeader {
    std::uint16_t fileType;
    std::uint32_t fileSize;
    std::uint16_t reserved1;
    std::uint16_t reserved2;
    std::uint32_t offsetData;
};

struct BMPInfoHeader {
    std::uint32_t headerSize;
    std::int32_t width;
    std::int32_t height;
    std::uint16_t planes;
    std::uint16_t bitCount;
    std::uint32_t compression;
    std::uint32_t imageSize;
    std::int32_t xPixelsPerMeter;
    std::int32_t yPixelsPerMeter;
    std::uint32_t colorsUsed;
    std::uint32_t colorsImportant;
};
#pragma pack(pop)

constexpr std::uint16_t kBMPMagic = 0x4D42;
constexpr std::uint32_t kBI_RGB = 0;

std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

int paddedRowSize(int width) {
    const int rowStride = width * 3;
    return (rowStride + 3) & ~3;
}
} // namespace

BMPImage::BMPImage() : m_width(0), m_height(0) {}

BMPImage::BMPImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int BMPImage::width() const {
    return m_width;
}

int BMPImage::height() const {
    return m_height;
}

bool BMPImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& BMPImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void BMPImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool BMPImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        return false;
    }

    const int rowSize = paddedRowSize(m_width);
    const std::uint32_t imageSize = static_cast<std::uint32_t>(rowSize * m_height);

    BMPFileHeader fileHeader{};
    fileHeader.fileType = kBMPMagic;
    fileHeader.fileSize = static_cast<std::uint32_t>(sizeof(BMPFileHeader) + sizeof(BMPInfoHeader)) + imageSize;
    fileHeader.offsetData = static_cast<std::uint32_t>(sizeof(BMPFileHeader) + sizeof(BMPInfoHeader));

    BMPInfoHeader infoHeader{};
    infoHeader.headerSize = sizeof(BMPInfoHeader);
    infoHeader.width = m_width;
    infoHeader.height = m_height;
    infoHeader.planes = 1;
    infoHeader.bitCount = 24;
    infoHeader.compression = kBI_RGB;
    infoHeader.imageSize = imageSize;

    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    const int paddingSize = rowSize - (m_width * 3);
    const std::uint8_t padding[3] = {0, 0, 0};

    for (int y = m_height - 1; y >= 0; --y) {
        for (int x = 0; x < m_width; ++x) {
            const Color& px = m_pixels[pixelIndex(x, y, m_width)];
            const std::uint8_t bgr[3] = {px.b, px.g, px.r};
            out.write(reinterpret_cast<const char*>(bgr), 3);
        }
        out.write(reinterpret_cast<const char*>(padding), paddingSize);
    }

    return static_cast<bool>(out);
}

BMPImage BMPImage::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open BMP file: " + filename);
    }

    BMPFileHeader fileHeader{};
    BMPInfoHeader infoHeader{};

    in.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    in.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));

    if (!in) {
        throw std::runtime_error("Failed to read BMP headers");
    }
    if (fileHeader.fileType != kBMPMagic) {
        throw std::runtime_error("Not a BMP file");
    }
    if (infoHeader.headerSize != sizeof(BMPInfoHeader)) {
        throw std::runtime_error("Unsupported BMP info header size");
    }
    if (infoHeader.bitCount != 24 || infoHeader.compression != kBI_RGB) {
        throw std::runtime_error("Only uncompressed 24-bit BMP is supported");
    }
    if (infoHeader.width <= 0 || infoHeader.height == 0) {
        throw std::runtime_error("Invalid BMP dimensions");
    }

    const int width = infoHeader.width;
    const bool topDown = infoHeader.height < 0;
    const int height = topDown ? -infoHeader.height : infoHeader.height;

    BMPImage image(width, height, Color(0, 0, 0));

    const int rowSize = paddedRowSize(width);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(rowSize));

    in.seekg(fileHeader.offsetData, std::ios::beg);

    for (int fileY = 0; fileY < height; ++fileY) {
        in.read(reinterpret_cast<char*>(row.data()), rowSize);
        if (!in) {
            throw std::runtime_error("Unexpected end of BMP pixel data");
        }

        const int y = topDown ? fileY : (height - 1 - fileY);
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(x) * 3;
            image.setPixel(x, y, Color(row[i + 2], row[i + 1], row[i]));
        }
    }

    return image;
}
