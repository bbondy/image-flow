#include "jpg.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;

constexpr std::array<int, 64> kZigZag = {
    0,  1,  8, 16, 9,  2,  3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63};

constexpr std::array<std::uint8_t, 64> kQuantLuma = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99};

constexpr std::array<std::uint8_t, 64> kQuantChroma = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99};

constexpr std::array<std::uint8_t, 17> kDcLumaBits = {
    0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00};

constexpr std::array<std::uint8_t, 12> kDcLumaVals = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr std::array<std::uint8_t, 17> kAcLumaBits = {
    0x00, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04,
    0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01,
    0x7D};

constexpr std::array<std::uint8_t, 162> kAcLumaVals = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA};

constexpr std::array<std::uint8_t, 17> kDcChromaBits = {
    0x00, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00};

constexpr std::array<std::uint8_t, 12> kDcChromaVals = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr std::array<std::uint8_t, 17> kAcChromaBits = {
    0x00, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03,
    0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02,
    0x77};

constexpr std::array<std::uint8_t, 162> kAcChromaVals = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
    0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
    0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
    0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
    0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA};

std::uint8_t clampToByte(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return static_cast<std::uint8_t>(v);
}

std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

void writeMarker(std::vector<std::uint8_t>& out, std::uint8_t marker) {
    out.push_back(0xFF);
    out.push_back(marker);
}

void writeU16BE(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

struct HuffmanTable {
    std::array<std::uint8_t, 17> bits{};
    std::vector<std::uint8_t> values;
    std::array<int, 17> minCode{};
    std::array<int, 17> maxCode{};
    std::array<int, 17> valPtr{};
    std::array<std::uint16_t, 256> code{};
    std::array<std::uint8_t, 256> codeLen{};
    bool defined = false;
};

void buildHuffmanTable(HuffmanTable& table) {
    int code = 0;
    int k = 0;
    for (int i = 1; i <= 16; ++i) {
        if (table.bits[i] == 0) {
            table.minCode[i] = -1;
            table.maxCode[i] = -1;
            table.valPtr[i] = -1;
        } else {
            table.minCode[i] = code;
            table.valPtr[i] = k;
            code += table.bits[i] - 1;
            table.maxCode[i] = code;
            ++code;
            k += table.bits[i];
        }
        code <<= 1;
    }

    table.code.fill(0);
    table.codeLen.fill(0);

    code = 0;
    k = 0;
    for (int i = 1; i <= 16; ++i) {
        for (int j = 0; j < table.bits[i]; ++j) {
            const std::uint8_t symbol = table.values[static_cast<std::size_t>(k++)];
            table.code[symbol] = static_cast<std::uint16_t>(code);
            table.codeLen[symbol] = static_cast<std::uint8_t>(i);
            ++code;
        }
        code <<= 1;
    }
}

HuffmanTable makeHuffmanTable(const std::array<std::uint8_t, 17>& bits, const std::uint8_t* values, std::size_t valueCount) {
    HuffmanTable table;
    table.bits = bits;
    table.values.assign(values, values + valueCount);
    table.defined = true;
    buildHuffmanTable(table);
    return table;
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : m_out(out), m_acc(0), m_bits(0) {}

    void putBits(std::uint16_t bits, int bitCount) {
        for (int i = bitCount - 1; i >= 0; --i) {
            const std::uint8_t bit = static_cast<std::uint8_t>((bits >> i) & 1U);
            m_acc = static_cast<std::uint8_t>((m_acc << 1) | bit);
            ++m_bits;
            if (m_bits == 8) {
                flushByte(m_acc);
                m_bits = 0;
                m_acc = 0;
            }
        }
    }

    void flush() {
        if (m_bits > 0) {
            const std::uint8_t padded = static_cast<std::uint8_t>((m_acc << (8 - m_bits)) | ((1U << (8 - m_bits)) - 1U));
            flushByte(padded);
            m_bits = 0;
            m_acc = 0;
        }
    }

private:
    void flushByte(std::uint8_t byte) {
        m_out.push_back(byte);
        if (byte == 0xFF) {
            m_out.push_back(0x00);
        }
    }

    std::vector<std::uint8_t>& m_out;
    std::uint8_t m_acc;
    int m_bits;
};

class BitReader {
public:
    explicit BitReader(const std::vector<std::uint8_t>& data)
        : m_data(data), m_pos(0), m_bitsLeft(0), m_cur(0), m_hitMarker(false) {}

    std::uint32_t readBit() {
        if (m_bitsLeft == 0) {
            fillByte();
        }
        --m_bitsLeft;
        return (m_cur >> m_bitsLeft) & 1U;
    }

    std::uint32_t readBits(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            v = (v << 1) | readBit();
        }
        return v;
    }

    bool hitMarker() const {
        return m_hitMarker;
    }

private:
    void fillByte() {
        if (m_pos >= m_data.size()) {
            throw std::runtime_error("Unexpected end of JPEG scan data");
        }

        std::uint8_t byte = m_data[m_pos++];
        if (byte == 0xFF) {
            while (m_pos < m_data.size() && m_data[m_pos] == 0xFF) {
                ++m_pos;
            }
            if (m_pos >= m_data.size()) {
                throw std::runtime_error("Unexpected end of JPEG marker");
            }

            const std::uint8_t next = m_data[m_pos];
            if (next == 0x00) {
                ++m_pos;
                byte = 0xFF;
            } else if (next >= 0xD0 && next <= 0xD7) {
                ++m_pos;
                fillByte();
                return;
            } else {
                m_hitMarker = true;
                throw std::runtime_error("Unexpected marker in JPEG scan");
            }
        }

        m_cur = byte;
        m_bitsLeft = 8;
    }

    const std::vector<std::uint8_t>& m_data;
    std::size_t m_pos;
    int m_bitsLeft;
    std::uint8_t m_cur;
    bool m_hitMarker;
};

int magnitudeCategory(int value) {
    if (value == 0) {
        return 0;
    }
    int absVal = value > 0 ? value : -value;
    int category = 0;
    while (absVal > 0) {
        ++category;
        absVal >>= 1;
    }
    return category;
}

std::uint16_t magnitudeBits(int value, int category) {
    if (category == 0) {
        return 0;
    }
    if (value >= 0) {
        return static_cast<std::uint16_t>(value);
    }
    return static_cast<std::uint16_t>((1 << category) - 1 + value);
}

int extendSign(std::uint32_t bits, int category) {
    if (category == 0) {
        return 0;
    }
    const int vt = 1 << (category - 1);
    if (static_cast<int>(bits) >= vt) {
        return static_cast<int>(bits);
    }
    return static_cast<int>(bits) - ((1 << category) - 1);
}

std::array<double, 64> fdct8x8(const std::array<double, 64>& in) {
    std::array<double, 64> out{};

    for (int v = 0; v < 8; ++v) {
        for (int u = 0; u < 8; ++u) {
            double sum = 0.0;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    sum += in[static_cast<std::size_t>(y) * 8 + x] *
                           std::cos(((2.0 * x + 1.0) * u * kPi) / 16.0) *
                           std::cos(((2.0 * y + 1.0) * v * kPi) / 16.0);
                }
            }
            const double cu = (u == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
            const double cv = (v == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
            out[static_cast<std::size_t>(v) * 8 + u] = 0.25 * cu * cv * sum;
        }
    }

    return out;
}

std::array<double, 64> idct8x8(const std::array<double, 64>& in) {
    std::array<double, 64> out{};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            double sum = 0.0;
            for (int v = 0; v < 8; ++v) {
                for (int u = 0; u < 8; ++u) {
                    const double cu = (u == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                    const double cv = (v == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                    sum += cu * cv * in[static_cast<std::size_t>(v) * 8 + u] *
                           std::cos(((2.0 * x + 1.0) * u * kPi) / 16.0) *
                           std::cos(((2.0 * y + 1.0) * v * kPi) / 16.0);
                }
            }
            out[static_cast<std::size_t>(y) * 8 + x] = 0.25 * sum;
        }
    }

    return out;
}

void emitDQT(std::vector<std::uint8_t>& out) {
    writeMarker(out, 0xDB);
    writeU16BE(out, 2 + (1 + 64) * 2);

    out.push_back(0x00);
    for (int i = 0; i < 64; ++i) {
        out.push_back(kQuantLuma[static_cast<std::size_t>(kZigZag[i])]);
    }

    out.push_back(0x01);
    for (int i = 0; i < 64; ++i) {
        out.push_back(kQuantChroma[static_cast<std::size_t>(kZigZag[i])]);
    }
}

void emitDHT(std::vector<std::uint8_t>& out) {
    writeMarker(out, 0xC4);
    const std::uint16_t len = static_cast<std::uint16_t>(2 +
        (1 + 16 + kDcLumaVals.size()) +
        (1 + 16 + kAcLumaVals.size()) +
        (1 + 16 + kDcChromaVals.size()) +
        (1 + 16 + kAcChromaVals.size()));
    writeU16BE(out, len);

    out.push_back(0x00);
    for (int i = 1; i <= 16; ++i) {
        out.push_back(kDcLumaBits[static_cast<std::size_t>(i)]);
    }
    out.insert(out.end(), kDcLumaVals.begin(), kDcLumaVals.end());

    out.push_back(0x10);
    for (int i = 1; i <= 16; ++i) {
        out.push_back(kAcLumaBits[static_cast<std::size_t>(i)]);
    }
    out.insert(out.end(), kAcLumaVals.begin(), kAcLumaVals.end());

    out.push_back(0x01);
    for (int i = 1; i <= 16; ++i) {
        out.push_back(kDcChromaBits[static_cast<std::size_t>(i)]);
    }
    out.insert(out.end(), kDcChromaVals.begin(), kDcChromaVals.end());

    out.push_back(0x11);
    for (int i = 1; i <= 16; ++i) {
        out.push_back(kAcChromaBits[static_cast<std::size_t>(i)]);
    }
    out.insert(out.end(), kAcChromaVals.begin(), kAcChromaVals.end());
}

int decodeHuffmanSymbol(BitReader& br, const HuffmanTable& ht) {
    int code = 0;
    for (int len = 1; len <= 16; ++len) {
        code = (code << 1) | static_cast<int>(br.readBit());
        if (ht.minCode[len] >= 0 && code <= ht.maxCode[len]) {
            const int idx = ht.valPtr[len] + (code - ht.minCode[len]);
            if (idx < 0 || idx >= static_cast<int>(ht.values.size())) {
                throw std::runtime_error("Corrupt Huffman table");
            }
            return ht.values[static_cast<std::size_t>(idx)];
        }
    }
    throw std::runtime_error("Invalid Huffman code");
}

std::vector<std::uint8_t> readFileBytes(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open JPEG file: " + filename);
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

JPGImage::JPGImage() : m_width(0), m_height(0) {}

JPGImage::JPGImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int JPGImage::width() const {
    return m_width;
}

int JPGImage::height() const {
    return m_height;
}

bool JPGImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& JPGImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void JPGImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool JPGImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    HuffmanTable dcY = makeHuffmanTable(kDcLumaBits, kDcLumaVals.data(), kDcLumaVals.size());
    HuffmanTable acY = makeHuffmanTable(kAcLumaBits, kAcLumaVals.data(), kAcLumaVals.size());
    HuffmanTable dcC = makeHuffmanTable(kDcChromaBits, kDcChromaVals.data(), kDcChromaVals.size());
    HuffmanTable acC = makeHuffmanTable(kAcChromaBits, kAcChromaVals.data(), kAcChromaVals.size());

    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height));

    writeMarker(out, 0xD8); // SOI

    // JFIF APP0
    writeMarker(out, 0xE0);
    writeU16BE(out, 16);
    out.push_back('J');
    out.push_back('F');
    out.push_back('I');
    out.push_back('F');
    out.push_back(0x00);
    out.push_back(0x01);
    out.push_back(0x01);
    out.push_back(0x00);
    writeU16BE(out, 1);
    writeU16BE(out, 1);
    out.push_back(0);
    out.push_back(0);

    emitDQT(out);

    writeMarker(out, 0xC0); // SOF0
    writeU16BE(out, 17);
    out.push_back(8);
    writeU16BE(out, static_cast<std::uint16_t>(m_height));
    writeU16BE(out, static_cast<std::uint16_t>(m_width));
    out.push_back(3);

    out.push_back(1);
    out.push_back(0x22);
    out.push_back(0);

    out.push_back(2);
    out.push_back(0x11);
    out.push_back(1);

    out.push_back(3);
    out.push_back(0x11);
    out.push_back(1);

    emitDHT(out);

    writeMarker(out, 0xDA); // SOS
    writeU16BE(out, 12);
    out.push_back(3);
    out.push_back(1);
    out.push_back(0x00);
    out.push_back(2);
    out.push_back(0x11);
    out.push_back(3);
    out.push_back(0x11);
    out.push_back(0);
    out.push_back(63);
    out.push_back(0);

    BitWriter bw(out);
    int prevDCY = 0;
    int prevDCCb = 0;
    int prevDCCr = 0;

    const int mcuW = (m_width + 15) / 16;
    const int mcuH = (m_height + 15) / 16;

    auto encodeBlock = [&](const std::array<double, 64>& spatial,
                           const std::array<std::uint8_t, 64>& q,
                           const HuffmanTable& dc,
                           const HuffmanTable& ac,
                           int& prevDC) {
        std::array<double, 64> freq = fdct8x8(spatial);

        std::array<int, 64> zz{};
        for (int i = 0; i < 64; ++i) {
            const int natural = kZigZag[static_cast<std::size_t>(i)];
            const double qv = static_cast<double>(q[static_cast<std::size_t>(natural)]);
            zz[static_cast<std::size_t>(i)] = static_cast<int>(std::lround(freq[static_cast<std::size_t>(natural)] / qv));
        }

        const int dcDiff = zz[0] - prevDC;
        prevDC = zz[0];

        const int dcCat = magnitudeCategory(dcDiff);
        bw.putBits(dc.code[static_cast<std::size_t>(dcCat)], dc.codeLen[static_cast<std::size_t>(dcCat)]);
        if (dcCat > 0) {
            bw.putBits(magnitudeBits(dcDiff, dcCat), dcCat);
        }

        int run = 0;
        for (int i = 1; i < 64; ++i) {
            const int coeff = zz[static_cast<std::size_t>(i)];
            if (coeff == 0) {
                ++run;
                continue;
            }

            while (run >= 16) {
                const std::uint8_t zrl = 0xF0;
                bw.putBits(ac.code[zrl], ac.codeLen[zrl]);
                run -= 16;
            }

            const int cat = magnitudeCategory(coeff);
            const std::uint8_t symbol = static_cast<std::uint8_t>((run << 4) | cat);
            bw.putBits(ac.code[symbol], ac.codeLen[symbol]);
            bw.putBits(magnitudeBits(coeff, cat), cat);
            run = 0;
        }

        if (run > 0) {
            const std::uint8_t eob = 0x00;
            bw.putBits(ac.code[eob], ac.codeLen[eob]);
        }
    };

    std::array<double, 64> blockY{};
    std::array<double, 64> blockCb{};
    std::array<double, 64> blockCr{};

    auto getYCbCr = [&](int x, int y, double& outY, double& outCb, double& outCr) {
        const int clampedX = std::min(std::max(x, 0), m_width - 1);
        const int clampedY = std::min(std::max(y, 0), m_height - 1);
        const Color& c = m_pixels[pixelIndex(clampedX, clampedY, m_width)];
        const double r = static_cast<double>(c.r);
        const double g = static_cast<double>(c.g);
        const double b = static_cast<double>(c.b);
        outY = 0.299 * r + 0.587 * g + 0.114 * b;
        outCb = -0.168736 * r - 0.331264 * g + 0.5 * b + 128.0;
        outCr = 0.5 * r - 0.418688 * g - 0.081312 * b + 128.0;
    };

    for (int my = 0; my < mcuH; ++my) {
        for (int mx = 0; mx < mcuW; ++mx) {
            // 4 Y blocks in a 16x16 MCU (4:2:0).
            for (int yBlock = 0; yBlock < 2; ++yBlock) {
                for (int xBlock = 0; xBlock < 2; ++xBlock) {
                    for (int by = 0; by < 8; ++by) {
                        for (int bx = 0; bx < 8; ++bx) {
                            const int x = mx * 16 + xBlock * 8 + bx;
                            const int y = my * 16 + yBlock * 8 + by;
                            double Y = 0.0;
                            double Cb = 0.0;
                            double Cr = 0.0;
                            getYCbCr(x, y, Y, Cb, Cr);
                            const std::size_t idx = static_cast<std::size_t>(by) * 8 + bx;
                            blockY[idx] = Y - 128.0;
                        }
                    }
                    encodeBlock(blockY, kQuantLuma, dcY, acY, prevDCY);
                }
            }

            // One Cb and one Cr block per 16x16 MCU by averaging 2x2 source samples.
            for (int by = 0; by < 8; ++by) {
                for (int bx = 0; bx < 8; ++bx) {
                    double cbSum = 0.0;
                    double crSum = 0.0;
                    for (int sy = 0; sy < 2; ++sy) {
                        for (int sx = 0; sx < 2; ++sx) {
                            const int x = mx * 16 + bx * 2 + sx;
                            const int y = my * 16 + by * 2 + sy;
                            double Y = 0.0;
                            double Cb = 0.0;
                            double Cr = 0.0;
                            getYCbCr(x, y, Y, Cb, Cr);
                            cbSum += Cb;
                            crSum += Cr;
                        }
                    }
                    const std::size_t idx = static_cast<std::size_t>(by) * 8 + bx;
                    blockCb[idx] = (cbSum * 0.25) - 128.0;
                    blockCr[idx] = (crSum * 0.25) - 128.0;
                }
            }

            encodeBlock(blockCb, kQuantChroma, dcC, acC, prevDCCb);
            encodeBlock(blockCr, kQuantChroma, dcC, acC, prevDCCr);
        }
    }

    bw.flush();
    writeMarker(out, 0xD9); // EOI

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(file);
}

JPGImage JPGImage::load(const std::string& filename) {
    const std::vector<std::uint8_t> bytes = readFileBytes(filename);
    if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) {
        throw std::runtime_error("Not a JPEG file");
    }

    std::array<std::array<std::uint8_t, 64>, 4> quantTables{};
    std::array<bool, 4> quantDefined{};

    std::array<HuffmanTable, 4> dcTables;
    std::array<HuffmanTable, 4> acTables;

    int width = 0;
    int height = 0;
    struct Component {
        int id = 0;
        int h = 1;
        int v = 1;
        int qt = 0;
        int dc = 0;
        int ac = 0;
    };
    std::array<Component, 3> components{};
    int compCount = 0;
    std::array<int, 3> scanOrder{};
    int scanCount = 0;

    std::size_t pos = 2;
    std::vector<std::uint8_t> scanData;

    while (pos + 1 < bytes.size()) {
        if (bytes[pos] != 0xFF) {
            ++pos;
            continue;
        }
        while (pos < bytes.size() && bytes[pos] == 0xFF) {
            ++pos;
        }
        if (pos >= bytes.size()) {
            break;
        }

        const std::uint8_t marker = bytes[pos++];
        if (marker == 0xD9) {
            break;
        }
        if (marker == 0xDA) {
            if (pos + 2 > bytes.size()) {
                throw std::runtime_error("Corrupt JPEG SOS");
            }
            const std::uint16_t segLen = (static_cast<std::uint16_t>(bytes[pos]) << 8) | bytes[pos + 1];
            if (segLen < 2 || pos + segLen > bytes.size()) {
                throw std::runtime_error("Invalid JPEG SOS length");
            }
            pos += 2;
            if (segLen < 8) {
                throw std::runtime_error("Unsupported JPEG SOS");
            }
            const int scanComps = bytes[pos++];
            if (scanComps != 3) {
                throw std::runtime_error("Only 3-component JPEG is supported");
            }
            scanCount = scanComps;
            for (int i = 0; i < scanComps; ++i) {
                const int cid = bytes[pos++];
                const int sel = bytes[pos++];
                bool found = false;
                for (int c = 0; c < compCount; ++c) {
                    if (components[static_cast<std::size_t>(c)].id == cid) {
                        components[static_cast<std::size_t>(c)].dc = (sel >> 4) & 0x0F;
                        components[static_cast<std::size_t>(c)].ac = sel & 0x0F;
                        scanOrder[static_cast<std::size_t>(i)] = c;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throw std::runtime_error("SOS references unknown JPEG component");
                }
            }
            pos += 3; // Ss, Se, AhAl

            const std::size_t scanStart = pos;
            std::size_t scanEnd = scanStart;
            while (scanEnd + 1 < bytes.size()) {
                if (bytes[scanEnd] == 0xFF && bytes[scanEnd + 1] != 0x00) {
                    if (bytes[scanEnd + 1] == 0xD9) {
                        break;
                    }
                    if (bytes[scanEnd + 1] >= 0xD0 && bytes[scanEnd + 1] <= 0xD7) {
                        scanEnd += 2;
                        continue;
                    }
                }
                ++scanEnd;
            }
            scanData.assign(bytes.begin() + static_cast<std::ptrdiff_t>(scanStart),
                            bytes.begin() + static_cast<std::ptrdiff_t>(scanEnd));
            break;
        }

        if (pos + 2 > bytes.size()) {
            throw std::runtime_error("Corrupt JPEG segment length");
        }
        const std::uint16_t segLen = (static_cast<std::uint16_t>(bytes[pos]) << 8) | bytes[pos + 1];
        if (segLen < 2 || pos + segLen > bytes.size()) {
            throw std::runtime_error("Invalid JPEG segment length");
        }
        pos += 2;

        const std::size_t segStart = pos;
        const std::size_t segDataLen = segLen - 2;

        if (marker == 0xDB) {
            std::size_t p = segStart;
            while (p < segStart + segDataLen) {
                const std::uint8_t pqTq = bytes[p++];
                const int precision = (pqTq >> 4) & 0x0F;
                const int tq = pqTq & 0x0F;
                if (precision != 0 || tq < 0 || tq > 3) {
                    throw std::runtime_error("Unsupported JPEG quantization table");
                }
                if (p + 64 > segStart + segDataLen) {
                    throw std::runtime_error("Corrupt JPEG DQT");
                }
                for (int i = 0; i < 64; ++i) {
                    quantTables[static_cast<std::size_t>(tq)][static_cast<std::size_t>(kZigZag[i])] = bytes[p++];
                }
                quantDefined[static_cast<std::size_t>(tq)] = true;
            }
        } else if (marker == 0xC0) {
            if (segDataLen < 6) {
                throw std::runtime_error("Corrupt JPEG SOF0");
            }
            const std::uint8_t precision = bytes[segStart];
            if (precision != 8) {
                throw std::runtime_error("Only 8-bit JPEG is supported");
            }
            height = (static_cast<int>(bytes[segStart + 1]) << 8) | bytes[segStart + 2];
            width = (static_cast<int>(bytes[segStart + 3]) << 8) | bytes[segStart + 4];
            compCount = bytes[segStart + 5];
            if (width <= 0 || height <= 0 || compCount != 3) {
                throw std::runtime_error("Only 3-component JPEG is supported");
            }
            if (segDataLen < static_cast<std::size_t>(6 + compCount * 3)) {
                throw std::runtime_error("Corrupt JPEG SOF0 components");
            }
            std::size_t p = segStart + 6;
            for (int i = 0; i < compCount; ++i) {
                components[static_cast<std::size_t>(i)].id = bytes[p++];
                const std::uint8_t hv = bytes[p++];
                components[static_cast<std::size_t>(i)].h = (hv >> 4) & 0x0F;
                components[static_cast<std::size_t>(i)].v = hv & 0x0F;
                components[static_cast<std::size_t>(i)].qt = bytes[p++];
                if (components[static_cast<std::size_t>(i)].h <= 0 || components[static_cast<std::size_t>(i)].v <= 0) {
                    throw std::runtime_error("Invalid JPEG sampling factors");
                }
            }
        } else if (marker == 0xC4) {
            std::size_t p = segStart;
            while (p < segStart + segDataLen) {
                const std::uint8_t tcTh = bytes[p++];
                const int tc = (tcTh >> 4) & 0x0F;
                const int th = tcTh & 0x0F;
                if (th < 0 || th > 3 || tc > 1) {
                    throw std::runtime_error("Unsupported JPEG Huffman table");
                }

                std::array<std::uint8_t, 17> bits{};
                int total = 0;
                for (int i = 1; i <= 16; ++i) {
                    if (p >= segStart + segDataLen) {
                        throw std::runtime_error("Corrupt JPEG DHT bits");
                    }
                    bits[static_cast<std::size_t>(i)] = bytes[p++];
                    total += bits[static_cast<std::size_t>(i)];
                }
                if (p + static_cast<std::size_t>(total) > segStart + segDataLen) {
                    throw std::runtime_error("Corrupt JPEG DHT values");
                }
                HuffmanTable ht;
                ht.bits = bits;
                ht.values.assign(bytes.begin() + static_cast<std::ptrdiff_t>(p),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(p + total));
                ht.defined = true;
                buildHuffmanTable(ht);
                p += static_cast<std::size_t>(total);

                if (tc == 0) {
                    dcTables[static_cast<std::size_t>(th)] = ht;
                } else {
                    acTables[static_cast<std::size_t>(th)] = ht;
                }
            }
        }

        pos = segStart + segDataLen;
    }

    if (width <= 0 || height <= 0 || scanData.empty()) {
        throw std::runtime_error("Incomplete JPEG file");
    }
    if (scanCount != 3) {
        throw std::runtime_error("Unsupported JPEG scan layout");
    }

    for (int i = 0; i < compCount; ++i) {
        const Component& c = components[static_cast<std::size_t>(i)];
        if (c.qt < 0 || c.qt > 3 || !quantDefined[static_cast<std::size_t>(c.qt)]) {
            throw std::runtime_error("Missing JPEG quantization table");
        }
        if (!dcTables[static_cast<std::size_t>(c.dc)].defined || !acTables[static_cast<std::size_t>(c.ac)].defined) {
            throw std::runtime_error("Missing JPEG Huffman table");
        }
    }

    int maxH = 1;
    int maxV = 1;
    for (int i = 0; i < compCount; ++i) {
        maxH = std::max(maxH, components[static_cast<std::size_t>(i)].h);
        maxV = std::max(maxV, components[static_cast<std::size_t>(i)].v);
    }
    if (maxH <= 0 || maxV <= 0 || maxH > 4 || maxV > 4) {
        throw std::runtime_error("Unsupported JPEG sampling factors");
    }
    for (int i = 0; i < compCount; ++i) {
        if ((maxH % components[static_cast<std::size_t>(i)].h) != 0 ||
            (maxV % components[static_cast<std::size_t>(i)].v) != 0) {
            throw std::runtime_error("Unsupported JPEG sampling ratio");
        }
    }

    JPGImage image(width, height, Color(0, 0, 0));

    BitReader br(scanData);
    int prevDC[3] = {0, 0, 0};
    const int mcuPixelW = maxH * 8;
    const int mcuPixelH = maxV * 8;
    const int mcuW = (width + mcuPixelW - 1) / mcuPixelW;
    const int mcuH = (height + mcuPixelH - 1) / mcuPixelH;

    std::array<std::vector<std::array<double, 64>>, 3> compSpatial{};

    auto decodeBlock = [&](int compIdx, std::array<double, 64>& outSpatial) {
        const Component& comp = components[static_cast<std::size_t>(compIdx)];
        const HuffmanTable& dc = dcTables[static_cast<std::size_t>(comp.dc)];
        const HuffmanTable& ac = acTables[static_cast<std::size_t>(comp.ac)];
        const auto& q = quantTables[static_cast<std::size_t>(comp.qt)];

        std::array<int, 64> zz{};

        const int dcLen = decodeHuffmanSymbol(br, dc);
        const int dcBits = (dcLen > 0) ? extendSign(br.readBits(dcLen), dcLen) : 0;
        prevDC[compIdx] += dcBits;
        zz[0] = prevDC[compIdx];

        int k = 1;
        while (k < 64) {
            const int symbol = decodeHuffmanSymbol(br, ac);
            if (symbol == 0x00) {
                while (k < 64) {
                    zz[static_cast<std::size_t>(k++)] = 0;
                }
                break;
            }
            if (symbol == 0xF0) {
                for (int i = 0; i < 16 && k < 64; ++i) {
                    zz[static_cast<std::size_t>(k++)] = 0;
                }
                continue;
            }

            const int run = (symbol >> 4) & 0x0F;
            const int size = symbol & 0x0F;
            for (int i = 0; i < run && k < 64; ++i) {
                zz[static_cast<std::size_t>(k++)] = 0;
            }
            if (k >= 64) {
                break;
            }
            const int acBits = extendSign(br.readBits(size), size);
            zz[static_cast<std::size_t>(k++)] = acBits;
        }

        std::array<double, 64> freq{};
        for (int i = 0; i < 64; ++i) {
            const int natural = kZigZag[static_cast<std::size_t>(i)];
            freq[static_cast<std::size_t>(natural)] =
                static_cast<double>(zz[static_cast<std::size_t>(i)] * static_cast<int>(q[static_cast<std::size_t>(natural)]));
        }

        outSpatial = idct8x8(freq);
    };

    for (int ci = 0; ci < compCount; ++ci) {
        const Component& c = components[static_cast<std::size_t>(ci)];
        compSpatial[static_cast<std::size_t>(ci)].resize(static_cast<std::size_t>(c.h * c.v));
    }

    int idxY = 0;
    int idxCb = 1;
    int idxCr = 2;
    for (int i = 0; i < compCount; ++i) {
        if (components[static_cast<std::size_t>(i)].id == 1) {
            idxY = i;
        } else if (components[static_cast<std::size_t>(i)].id == 2) {
            idxCb = i;
        } else if (components[static_cast<std::size_t>(i)].id == 3) {
            idxCr = i;
        }
    }

    auto sampleFromComponent = [&](int compIdx, int localX, int localY) -> double {
        const Component& c = components[static_cast<std::size_t>(compIdx)];
        const int scaleX = maxH / c.h;
        const int scaleY = maxV / c.v;
        int cx = localX / scaleX;
        int cy = localY / scaleY;
        const int maxCX = c.h * 8 - 1;
        const int maxCY = c.v * 8 - 1;
        cx = std::min(std::max(cx, 0), maxCX);
        cy = std::min(std::max(cy, 0), maxCY);
        const int blockX = cx / 8;
        const int blockY = cy / 8;
        const int blockIdx = blockY * c.h + blockX;
        const int inX = cx % 8;
        const int inY = cy % 8;
        return compSpatial[static_cast<std::size_t>(compIdx)][static_cast<std::size_t>(blockIdx)]
                          [static_cast<std::size_t>(inY) * 8 + inX];
    };

    for (int my = 0; my < mcuH; ++my) {
        for (int mx = 0; mx < mcuW; ++mx) {
            for (int s = 0; s < scanCount; ++s) {
                const int compIdx = scanOrder[static_cast<std::size_t>(s)];
                const Component& c = components[static_cast<std::size_t>(compIdx)];
                for (int vy = 0; vy < c.v; ++vy) {
                    for (int hx = 0; hx < c.h; ++hx) {
                        const int blockIdx = vy * c.h + hx;
                        decodeBlock(compIdx, compSpatial[static_cast<std::size_t>(compIdx)][static_cast<std::size_t>(blockIdx)]);
                    }
                }
            }

            for (int by = 0; by < mcuPixelH; ++by) {
                for (int bx = 0; bx < mcuPixelW; ++bx) {
                    const int x = mx * mcuPixelW + bx;
                    const int y = my * mcuPixelH + by;
                    if (x >= width || y >= height) {
                        continue;
                    }
                    const double Y = sampleFromComponent(idxY, bx, by) + 128.0;
                    const double Cb = sampleFromComponent(idxCb, bx, by) + 128.0;
                    const double Cr = sampleFromComponent(idxCr, bx, by) + 128.0;

                    const int r = static_cast<int>(std::lround(Y + 1.402 * (Cr - 128.0)));
                    const int g = static_cast<int>(std::lround(Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0)));
                    const int b = static_cast<int>(std::lround(Y + 1.772 * (Cb - 128.0)));

                    image.setPixel(x, y, Color(clampToByte(r), clampToByte(g), clampToByte(b)));
                }
            }
        }
    }

    return image;
}
