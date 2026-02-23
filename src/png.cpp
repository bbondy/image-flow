#include "png.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint8_t kPNGSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
constexpr std::size_t kMaxImagePixels = 100000000;

std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

void validatePNGDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid PNG dimensions");
    }
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h || (w * h) > kMaxImagePixels) {
        throw std::runtime_error("Unsupported PNG dimensions");
    }
}

std::uint32_t readU32BE(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

void writeU32BE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
    static std::array<std::uint32_t, 256> table{};
    static bool initialized = false;
    if (!initialized) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }

    std::uint32_t c = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < len; ++i) {
        c = table[(c ^ data[i]) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t len) {
    constexpr std::uint32_t mod = 65521;
    std::uint32_t a = 1;
    std::uint32_t b = 0;

    for (std::size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % mod;
        b = (b + a) % mod;
    }
    return (b << 16) | a;
}

void appendChunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data) {
    writeU32BE(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t chunkStart = out.size();
    out.push_back(static_cast<std::uint8_t>(type[0]));
    out.push_back(static_cast<std::uint8_t>(type[1]));
    out.push_back(static_cast<std::uint8_t>(type[2]));
    out.push_back(static_cast<std::uint8_t>(type[3]));
    out.insert(out.end(), data.begin(), data.end());
    const std::uint32_t crc = crc32(out.data() + chunkStart, 4 + data.size());
    writeU32BE(out, crc);
}

std::vector<std::uint8_t> zlibCompressStored(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> out;
    out.reserve(input.size() + input.size() / 65535 + 16);

    out.push_back(0x78); // CMF: deflate, 32K window
    out.push_back(0x01); // FLG: fastest/low compression

    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t remaining = input.size() - offset;
        const std::uint16_t blockLen = static_cast<std::uint16_t>(remaining > 65535 ? 65535 : remaining);
        const bool finalBlock = (offset + blockLen == input.size());

        out.push_back(finalBlock ? 0x01 : 0x00); // BFINAL + BTYPE=00 with padding

        const std::uint16_t nlen = static_cast<std::uint16_t>(~blockLen);
        out.push_back(static_cast<std::uint8_t>(blockLen & 0xFF));
        out.push_back(static_cast<std::uint8_t>((blockLen >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
        out.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));

        out.insert(out.end(), input.begin() + static_cast<std::ptrdiff_t>(offset),
                   input.begin() + static_cast<std::ptrdiff_t>(offset + blockLen));
        offset += blockLen;
    }

    const std::uint32_t adler = adler32(input.data(), input.size());
    writeU32BE(out, adler);
    return out;
}

class BitReader {
public:
    BitReader(const std::uint8_t* bytes, std::size_t length)
        : m_bytes(bytes), m_length(length), m_pos(0), m_bit(0) {}

    std::uint32_t readBits(int count) {
        std::uint32_t value = 0;
        for (int i = 0; i < count; ++i) {
            if (m_pos >= m_length) {
                throw std::runtime_error("Unexpected end of deflate stream");
            }
            const std::uint8_t current = m_bytes[m_pos];
            const std::uint32_t bitVal = (current >> m_bit) & 1U;
            value |= (bitVal << i);

            ++m_bit;
            if (m_bit == 8) {
                m_bit = 0;
                ++m_pos;
            }
        }
        return value;
    }

    void alignToByte() {
        if (m_bit != 0) {
            m_bit = 0;
            ++m_pos;
        }
    }

    std::uint8_t readByteAligned() {
        if (m_bit != 0) {
            throw std::runtime_error("Deflate reader not aligned");
        }
        if (m_pos >= m_length) {
            throw std::runtime_error("Unexpected end of deflate stream");
        }
        return m_bytes[m_pos++];
    }

private:
    const std::uint8_t* m_bytes;
    std::size_t m_length;
    std::size_t m_pos;
    int m_bit;
};

std::vector<std::uint8_t> zlibDecompressStoredOnly(const std::vector<std::uint8_t>& input) {
    if (input.size() < 6) {
        throw std::runtime_error("Invalid zlib stream");
    }

    const std::uint8_t cmf = input[0];
    const std::uint8_t flg = input[1];
    if ((cmf & 0x0F) != 8) {
        throw std::runtime_error("Unsupported zlib compression method");
    }
    if (((static_cast<int>(cmf) << 8) + static_cast<int>(flg)) % 31 != 0) {
        throw std::runtime_error("Corrupt zlib header");
    }
    if ((flg & 0x20) != 0) {
        throw std::runtime_error("Preset dictionary not supported");
    }

    const std::size_t deflateOffset = 2;
    const std::size_t adlerOffset = input.size() - 4;
    BitReader br(input.data() + deflateOffset, adlerOffset - deflateOffset);

    std::vector<std::uint8_t> out;

    bool finalBlock = false;
    while (!finalBlock) {
        finalBlock = br.readBits(1) != 0;
        const std::uint32_t btype = br.readBits(2);
        if (btype != 0) {
            throw std::runtime_error("Only deflate stored blocks are supported");
        }

        br.alignToByte();
        const std::uint16_t len = static_cast<std::uint16_t>(br.readByteAligned()) |
                                  (static_cast<std::uint16_t>(br.readByteAligned()) << 8);
        const std::uint16_t nlen = static_cast<std::uint16_t>(br.readByteAligned()) |
                                   (static_cast<std::uint16_t>(br.readByteAligned()) << 8);
        if (static_cast<std::uint16_t>(~len) != nlen) {
            throw std::runtime_error("Corrupt deflate stored block");
        }

        for (std::uint16_t i = 0; i < len; ++i) {
            out.push_back(br.readByteAligned());
        }
    }

    const std::uint32_t expectedAdler = readU32BE(input, adlerOffset);
    const std::uint32_t actualAdler = adler32(out.data(), out.size());
    if (expectedAdler != actualAdler) {
        throw std::runtime_error("zlib Adler-32 mismatch");
    }

    return out;
}

std::uint8_t paethPredictor(std::uint8_t a, std::uint8_t b, std::uint8_t c) {
    const int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    const int pa = (p > static_cast<int>(a)) ? (p - static_cast<int>(a)) : (static_cast<int>(a) - p);
    const int pb = (p > static_cast<int>(b)) ? (p - static_cast<int>(b)) : (static_cast<int>(b) - p);
    const int pc = (p > static_cast<int>(c)) ? (p - static_cast<int>(c)) : (static_cast<int>(c) - p);

    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

std::vector<std::uint8_t> unfilterScanlines(const std::vector<std::uint8_t>& filtered, int width, int height, int bpp) {
    const std::size_t rowBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(bpp);
    const std::size_t expected = static_cast<std::size_t>(height) * (1 + rowBytes);
    if (filtered.size() != expected) {
        throw std::runtime_error("Unexpected PNG scanline size");
    }

    std::vector<std::uint8_t> out(static_cast<std::size_t>(height) * rowBytes);
    std::vector<std::uint8_t> prevRow(rowBytes, 0);
    std::vector<std::uint8_t> curRow(rowBytes, 0);

    std::size_t src = 0;
    std::size_t dst = 0;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t filter = filtered[src++];
        for (std::size_t x = 0; x < rowBytes; ++x) {
            const std::uint8_t raw = filtered[src++];
            const std::uint8_t a = (x >= static_cast<std::size_t>(bpp)) ? curRow[x - bpp] : 0;
            const std::uint8_t b = prevRow[x];
            const std::uint8_t c = (x >= static_cast<std::size_t>(bpp)) ? prevRow[x - bpp] : 0;

            std::uint8_t recon = 0;
            switch (filter) {
                case 0:
                    recon = raw;
                    break;
                case 1:
                    recon = static_cast<std::uint8_t>(raw + a);
                    break;
                case 2:
                    recon = static_cast<std::uint8_t>(raw + b);
                    break;
                case 3:
                    recon = static_cast<std::uint8_t>(raw + static_cast<std::uint8_t>((static_cast<int>(a) + static_cast<int>(b)) / 2));
                    break;
                case 4:
                    recon = static_cast<std::uint8_t>(raw + paethPredictor(a, b, c));
                    break;
                default:
                    throw std::runtime_error("Unsupported PNG filter type");
            }
            curRow[x] = recon;
        }

        for (std::size_t x = 0; x < rowBytes; ++x) {
            out[dst++] = curRow[x];
        }
        prevRow.swap(curRow);
    }

    return out;
}
} // namespace

PNGImage::PNGImage() : m_width(0), m_height(0) {}

PNGImage::PNGImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int PNGImage::width() const {
    return m_width;
}

int PNGImage::height() const {
    return m_height;
}

bool PNGImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& PNGImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void PNGImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool PNGImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    std::vector<std::uint8_t> file;
    file.insert(file.end(), kPNGSignature, kPNGSignature + 8);

    std::vector<std::uint8_t> ihdr;
    ihdr.reserve(13);
    writeU32BE(ihdr, static_cast<std::uint32_t>(m_width));
    writeU32BE(ihdr, static_cast<std::uint32_t>(m_height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // color type RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    appendChunk(file, "IHDR", ihdr);

    std::vector<std::uint8_t> raw;
    const std::size_t rowBytes = static_cast<std::size_t>(m_width) * 3;
    raw.reserve(static_cast<std::size_t>(m_height) * (1 + rowBytes));

    for (int y = 0; y < m_height; ++y) {
        raw.push_back(0); // filter type 0
        for (int x = 0; x < m_width; ++x) {
            const Color& px = m_pixels[pixelIndex(x, y, m_width)];
            raw.push_back(px.r);
            raw.push_back(px.g);
            raw.push_back(px.b);
        }
    }

    std::vector<std::uint8_t> compressed = zlibCompressStored(raw);
    appendChunk(file, "IDAT", compressed);
    appendChunk(file, "IEND", {});

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    return static_cast<bool>(out);
}

PNGImage PNGImage::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open PNG file: " + filename);
    }

    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 8 || !std::equal(kPNGSignature, kPNGSignature + 8, bytes.begin())) {
        throw std::runtime_error("Not a PNG file");
    }

    std::size_t pos = 8;
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = -1;
    int interlace = 0;
    bool gotIHDR = false;
    bool gotIEND = false;
    std::vector<std::uint8_t> idat;

    while (pos + 12 <= bytes.size()) {
        const std::uint32_t length = readU32BE(bytes, pos);
        pos += 4;

        if (pos + 4 + static_cast<std::size_t>(length) + 4 > bytes.size()) {
            throw std::runtime_error("Corrupt PNG chunk length");
        }

        const std::size_t chunkStart = pos;
        const char type[5] = {
            static_cast<char>(bytes[pos]),
            static_cast<char>(bytes[pos + 1]),
            static_cast<char>(bytes[pos + 2]),
            static_cast<char>(bytes[pos + 3]),
            '\0'};
        pos += 4;

        const std::uint8_t* dataPtr = bytes.data() + pos;
        const std::size_t dataSize = length;

        const std::uint32_t expectedCRC = readU32BE(bytes, pos + dataSize);
        const std::uint32_t actualCRC = crc32(bytes.data() + chunkStart, 4 + dataSize);
        if (expectedCRC != actualCRC) {
            throw std::runtime_error("PNG CRC mismatch");
        }

        if (std::string(type) == "IHDR") {
            if (length != 13) {
                throw std::runtime_error("Invalid IHDR size");
            }
            width = static_cast<int>(readU32BE(bytes, pos));
            height = static_cast<int>(readU32BE(bytes, pos + 4));
            bitDepth = bytes[pos + 8];
            colorType = bytes[pos + 9];
            const std::uint8_t compression = bytes[pos + 10];
            const std::uint8_t filterMethod = bytes[pos + 11];
            interlace = bytes[pos + 12];

            validatePNGDimensions(width, height);
            if (bitDepth != 8 || colorType != 2) {
                throw std::runtime_error("Only 24-bit RGB PNG is supported");
            }
            if (compression != 0 || filterMethod != 0 || interlace != 0) {
                throw std::runtime_error("Unsupported PNG compression/filter/interlace");
            }
            gotIHDR = true;
        } else if (std::string(type) == "IDAT") {
            idat.insert(idat.end(), dataPtr, dataPtr + dataSize);
        } else if (std::string(type) == "IEND") {
            gotIEND = true;
            pos += dataSize + 4;
            break;
        }

        pos += dataSize + 4;
    }

    if (!gotIHDR || !gotIEND) {
        throw std::runtime_error("PNG missing IHDR or IEND");
    }
    if (idat.empty()) {
        throw std::runtime_error("PNG missing IDAT");
    }

    std::vector<std::uint8_t> filtered = zlibDecompressStoredOnly(idat);
    std::vector<std::uint8_t> raw = unfilterScanlines(filtered, width, height, 3);

    PNGImage image(width, height, Color(0, 0, 0));
    std::size_t i = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.setPixel(x, y, Color(raw[i], raw[i + 1], raw[i + 2]));
            i += 3;
        }
    }

    return image;
}
