#include "gif.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
constexpr std::size_t kMaxImagePixels = 100000000;

std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

void validateGIFDimensions(int width, int height, const char* context) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error(std::string("Invalid ") + context + " dimensions");
    }
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h || (w * h) > kMaxImagePixels) {
        throw std::runtime_error(std::string("Unsupported ") + context + " dimensions");
    }
}

void writeU16LE(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

std::uint16_t readU16LE(const std::vector<std::uint8_t>& in, std::size_t pos) {
    return static_cast<std::uint16_t>(in[pos]) |
           (static_cast<std::uint16_t>(in[pos + 1]) << 8);
}

int ceilLog2(int v) {
    int n = 0;
    int p = 1;
    while (p < v) {
        p <<= 1;
        ++n;
    }
    return n;
}

class BitPackerLSB {
public:
    explicit BitPackerLSB(std::vector<std::uint8_t>& out) : m_out(out), m_cur(0), m_bits(0) {}

    void put(int code, int bits) {
        for (int i = 0; i < bits; ++i) {
            const int bit = (code >> i) & 1;
            m_cur |= static_cast<std::uint8_t>(bit << m_bits);
            ++m_bits;
            if (m_bits == 8) {
                m_out.push_back(m_cur);
                m_cur = 0;
                m_bits = 0;
            }
        }
    }

    void flush() {
        if (m_bits > 0) {
            m_out.push_back(m_cur);
            m_cur = 0;
            m_bits = 0;
        }
    }

private:
    std::vector<std::uint8_t>& m_out;
    std::uint8_t m_cur;
    int m_bits;
};

class BitReaderLSB {
public:
    explicit BitReaderLSB(const std::vector<std::uint8_t>& data)
        : m_data(data), m_bytePos(0), m_bitPos(0) {}

    bool read(int bits, int& out) {
        out = 0;
        for (int i = 0; i < bits; ++i) {
            if (m_bytePos >= m_data.size()) {
                return false;
            }
            const int bit = (m_data[m_bytePos] >> m_bitPos) & 1;
            out |= (bit << i);
            ++m_bitPos;
            if (m_bitPos == 8) {
                m_bitPos = 0;
                ++m_bytePos;
            }
        }
        return true;
    }

private:
    const std::vector<std::uint8_t>& m_data;
    std::size_t m_bytePos;
    int m_bitPos;
};

std::vector<std::uint8_t> lzwCompress(const std::vector<std::uint8_t>& indices, int minCodeSize) {
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(indices.size());
    BitPackerLSB bw(bytes);

    int nextCode = endCode + 1;
    int codeSize = minCodeSize + 1;
    bool haveOld = false;

    bw.put(clearCode, codeSize);

    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (haveOld && nextCode >= 4096) {
            bw.put(clearCode, codeSize);
            nextCode = endCode + 1;
            codeSize = minCodeSize + 1;
            haveOld = false;
        }

        bw.put(indices[i], codeSize);
        if (haveOld) {
            ++nextCode;
            if (nextCode == (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
        }
        haveOld = true;
    }

    bw.put(endCode, codeSize);
    bw.flush();
    return bytes;
}

std::vector<std::uint8_t> lzwDecompress(const std::vector<std::uint8_t>& data, int minCodeSize, std::size_t expectedPixels) {
    if (minCodeSize < 2 || minCodeSize > 8) {
        throw std::runtime_error("Unsupported GIF LZW code size");
    }

    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;

    std::array<int, 4096> prefix{};
    std::array<std::uint8_t, 4096> suffix{};
    std::array<std::uint8_t, 4096> stack{};

    auto resetTable = [&]() {
        for (int i = 0; i < clearCode; ++i) {
            prefix[i] = -1;
            suffix[i] = static_cast<std::uint8_t>(i);
        }
    };

    resetTable();

    int nextCode = endCode + 1;
    int codeSize = minCodeSize + 1;
    int oldCode = -1;
    std::uint8_t firstChar = 0;

    BitReaderLSB br(data);
    std::vector<std::uint8_t> out;
    out.reserve(expectedPixels);

    while (true) {
        int code = 0;
        if (!br.read(codeSize, code)) {
            break;
        }

        if (code == clearCode) {
            resetTable();
            nextCode = endCode + 1;
            codeSize = minCodeSize + 1;
            oldCode = -1;
            continue;
        }
        if (code == endCode) {
            break;
        }

        if (oldCode < 0) {
            if (code >= clearCode) {
                throw std::runtime_error("Corrupt GIF LZW first code");
            }
            firstChar = static_cast<std::uint8_t>(code);
            out.push_back(firstChar);
            oldCode = code;
            continue;
        }

        int inCode = code;
        int top = 0;

        if (code == nextCode) {
            stack[top++] = firstChar;
            code = oldCode;
        } else if (code > nextCode) {
            throw std::runtime_error("Corrupt GIF LZW code");
        }

        while (code >= clearCode) {
            if (code == clearCode || code == endCode) {
                throw std::runtime_error("Corrupt GIF LZW prefix chain");
            }
            if (top >= 4096) {
                throw std::runtime_error("Corrupt GIF LZW string");
            }
            stack[top++] = suffix[code];
            code = prefix[code];
            if (code < 0) {
                throw std::runtime_error("Corrupt GIF LZW prefix");
            }
        }

        firstChar = static_cast<std::uint8_t>(code);
        stack[top++] = firstChar;

        while (top > 0) {
            out.push_back(stack[--top]);
        }

        if (nextCode < 4096) {
            prefix[nextCode] = oldCode;
            suffix[nextCode] = firstChar;
            ++nextCode;
            if (nextCode == (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
        }

        oldCode = inCode;
        if (out.size() >= expectedPixels) {
            break;
        }
    }

    if (out.size() < expectedPixels) {
        throw std::runtime_error("Truncated GIF image data");
    }

    out.resize(expectedPixels);
    return out;
}

void writeSubBlocks(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& bytes) {
    std::size_t pos = 0;
    while (pos < bytes.size()) {
        const std::size_t len = std::min<std::size_t>(255, bytes.size() - pos);
        out.push_back(static_cast<std::uint8_t>(len));
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                   bytes.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    }
    out.push_back(0x00);
}

std::vector<std::uint8_t> readSubBlocks(const std::vector<std::uint8_t>& bytes, std::size_t& pos) {
    std::vector<std::uint8_t> out;
    while (true) {
        if (pos >= bytes.size()) {
            throw std::runtime_error("Corrupt GIF sub-block stream");
        }
        const std::uint8_t len = bytes[pos++];
        if (len == 0) {
            break;
        }
        if (pos + len > bytes.size()) {
            throw std::runtime_error("Corrupt GIF sub-block length");
        }
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                   bytes.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    }
    return out;
}
} // namespace

GIFImage::GIFImage() : m_width(0), m_height(0) {}

GIFImage::GIFImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int GIFImage::width() const {
    return m_width;
}

int GIFImage::height() const {
    return m_height;
}

bool GIFImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& GIFImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void GIFImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool GIFImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    std::unordered_map<std::uint32_t, std::uint8_t> colorToIndex;
    std::vector<Color> palette;
    palette.reserve(256);
    std::vector<std::uint8_t> indices;
    indices.resize(m_pixels.size());

    for (std::size_t i = 0; i < m_pixels.size(); ++i) {
        const Color& c = m_pixels[i];
        const std::uint32_t key = (static_cast<std::uint32_t>(c.r) << 16) |
                                  (static_cast<std::uint32_t>(c.g) << 8) |
                                  static_cast<std::uint32_t>(c.b);
        const auto it = colorToIndex.find(key);
        if (it != colorToIndex.end()) {
            indices[i] = it->second;
            continue;
        }
        if (palette.size() >= 256) {
            return false;
        }
        const std::uint8_t idx = static_cast<std::uint8_t>(palette.size());
        palette.push_back(c);
        colorToIndex.emplace(key, idx);
        indices[i] = idx;
    }

    const int colorCount = static_cast<int>(palette.size());
    const int tableBits = std::max(1, ceilLog2(std::max(2, colorCount)));
    const int tableSize = 1 << tableBits;
    const int minCodeSize = std::max(2, tableBits);

    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height));

    out.push_back('G');
    out.push_back('I');
    out.push_back('F');
    out.push_back('8');
    out.push_back('9');
    out.push_back('a');

    writeU16LE(out, static_cast<std::uint16_t>(m_width));
    writeU16LE(out, static_cast<std::uint16_t>(m_height));
    const std::uint8_t packed = static_cast<std::uint8_t>(0x80 | (7 << 4) | (tableBits - 1));
    out.push_back(packed);
    out.push_back(0x00);
    out.push_back(0x00);

    for (int i = 0; i < tableSize; ++i) {
        if (i < colorCount) {
            out.push_back(palette[static_cast<std::size_t>(i)].r);
            out.push_back(palette[static_cast<std::size_t>(i)].g);
            out.push_back(palette[static_cast<std::size_t>(i)].b);
        } else {
            out.push_back(0);
            out.push_back(0);
            out.push_back(0);
        }
    }

    out.push_back(0x2C);
    writeU16LE(out, 0);
    writeU16LE(out, 0);
    writeU16LE(out, static_cast<std::uint16_t>(m_width));
    writeU16LE(out, static_cast<std::uint16_t>(m_height));
    out.push_back(0x00);

    out.push_back(static_cast<std::uint8_t>(minCodeSize));
    const std::vector<std::uint8_t> compressed = lzwCompress(indices, minCodeSize);
    writeSubBlocks(out, compressed);

    out.push_back(0x3B);

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(file);
}

GIFImage GIFImage::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open GIF file: " + filename);
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (bytes.size() < 13) {
        throw std::runtime_error("GIF file too small");
    }
    const bool sig87 = bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8' && bytes[4] == '7' && bytes[5] == 'a';
    const bool sig89 = bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8' && bytes[4] == '9' && bytes[5] == 'a';
    if (!sig87 && !sig89) {
        throw std::runtime_error("Not a GIF file");
    }

    std::size_t pos = 6;
    const int canvasW = readU16LE(bytes, pos);
    const int canvasH = readU16LE(bytes, pos + 2);
    const std::uint8_t lsdPacked = bytes[pos + 4];
    pos += 7;

    validateGIFDimensions(canvasW, canvasH, "GIF");

    std::vector<Color> globalPalette;
    if ((lsdPacked & 0x80) != 0) {
        const int sizeBits = (lsdPacked & 0x07) + 1;
        const int gctSize = 1 << sizeBits;
        if (pos + static_cast<std::size_t>(gctSize) * 3 > bytes.size()) {
            throw std::runtime_error("Corrupt GIF global color table");
        }
        globalPalette.resize(static_cast<std::size_t>(gctSize));
        for (int i = 0; i < gctSize; ++i) {
            globalPalette[static_cast<std::size_t>(i)] = Color(bytes[pos], bytes[pos + 1], bytes[pos + 2]);
            pos += 3;
        }
    }

    bool gotImage = false;
    GIFImage image(canvasW, canvasH, Color(0, 0, 0));

    while (pos < bytes.size()) {
        const std::uint8_t introducer = bytes[pos++];
        if (introducer == 0x3B) {
            break;
        }

        if (introducer == 0x21) {
            if (pos >= bytes.size()) {
                throw std::runtime_error("Corrupt GIF extension block");
            }
            ++pos;
            (void)readSubBlocks(bytes, pos);
            continue;
        }

        if (introducer != 0x2C) {
            throw std::runtime_error("Unsupported GIF block type");
        }

        if (pos + 9 > bytes.size()) {
            throw std::runtime_error("Corrupt GIF image descriptor");
        }
        const int left = readU16LE(bytes, pos);
        const int top = readU16LE(bytes, pos + 2);
        const int imageW = readU16LE(bytes, pos + 4);
        const int imageH = readU16LE(bytes, pos + 6);
        const std::uint8_t idPacked = bytes[pos + 8];
        pos += 9;

        validateGIFDimensions(imageW, imageH, "GIF image");

        std::vector<Color> palette = globalPalette;
        if ((idPacked & 0x80) != 0) {
            const int sizeBits = (idPacked & 0x07) + 1;
            const int lctSize = 1 << sizeBits;
            if (pos + static_cast<std::size_t>(lctSize) * 3 > bytes.size()) {
                throw std::runtime_error("Corrupt GIF local color table");
            }
            palette.resize(static_cast<std::size_t>(lctSize));
            for (int i = 0; i < lctSize; ++i) {
                palette[static_cast<std::size_t>(i)] = Color(bytes[pos], bytes[pos + 1], bytes[pos + 2]);
                pos += 3;
            }
        }

        if (palette.empty()) {
            throw std::runtime_error("GIF has no color table");
        }

        if (pos >= bytes.size()) {
            throw std::runtime_error("Corrupt GIF LZW header");
        }
        const int minCodeSize = bytes[pos++];
        const std::vector<std::uint8_t> compressed = readSubBlocks(bytes, pos);
        const std::vector<std::uint8_t> indices =
            lzwDecompress(compressed, minCodeSize, static_cast<std::size_t>(imageW) * static_cast<std::size_t>(imageH));

        const bool interlaced = (idPacked & 0x40) != 0;
        std::size_t src = 0;
        if (!interlaced) {
            for (int y = 0; y < imageH; ++y) {
                for (int x = 0; x < imageW; ++x) {
                    const std::uint8_t idx = indices[src++];
                    if (idx >= palette.size()) {
                        continue;
                    }
                    image.setPixel(left + x, top + y, palette[idx]);
                }
            }
        } else {
            const int starts[4] = {0, 4, 2, 1};
            const int steps[4] = {8, 8, 4, 2};
            for (int pass = 0; pass < 4; ++pass) {
                for (int y = starts[pass]; y < imageH; y += steps[pass]) {
                    for (int x = 0; x < imageW; ++x) {
                        if (src >= indices.size()) {
                            break;
                        }
                        const std::uint8_t idx = indices[src++];
                        if (idx >= palette.size()) {
                            continue;
                        }
                        image.setPixel(left + x, top + y, palette[idx]);
                    }
                }
            }
        }

        gotImage = true;
        break;
    }

    if (!gotImage) {
        throw std::runtime_error("GIF file has no image frame");
    }

    return image;
}
