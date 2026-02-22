#include "steganography.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
std::size_t capacityBits(const RasterImage& image) {
    return static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.height()) * 3;
}

void pushBits(std::vector<std::uint8_t>& bits, std::uint32_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        bits.push_back(static_cast<std::uint8_t>((value >> i) & 1U));
    }
}

void setChannelLSB(std::uint8_t& channel, std::uint8_t bit) {
    channel = static_cast<std::uint8_t>((channel & 0xFEU) | (bit & 1U));
}

std::uint8_t getChannelLSB(std::uint8_t channel) {
    return static_cast<std::uint8_t>(channel & 1U);
}
} // namespace

Steganography::Steganography(RasterImage& image) : m_image(image) {}

std::size_t Steganography::capacityBytes(const RasterImage& image) {
    const std::size_t bits = capacityBits(image);
    if (bits <= 32) {
        return 0;
    }
    return (bits - 32) / 8;
}

bool Steganography::encodeMessage(const std::string& message) {
    const std::size_t cap = capacityBytes(m_image);
    if (message.size() > cap) {
        return false;
    }

    std::vector<std::uint8_t> bits;
    bits.reserve(32 + message.size() * 8);

    pushBits(bits, static_cast<std::uint32_t>(message.size()), 32);
    for (unsigned char c : message) {
        pushBits(bits, static_cast<std::uint32_t>(c), 8);
    }

    std::size_t bitIndex = 0;
    for (int y = 0; y < m_image.height() && bitIndex < bits.size(); ++y) {
        for (int x = 0; x < m_image.width() && bitIndex < bits.size(); ++x) {
            Color px = m_image.getPixel(x, y);

            if (bitIndex < bits.size()) {
                setChannelLSB(px.r, bits[bitIndex++]);
            }
            if (bitIndex < bits.size()) {
                setChannelLSB(px.g, bits[bitIndex++]);
            }
            if (bitIndex < bits.size()) {
                setChannelLSB(px.b, bits[bitIndex++]);
            }

            m_image.setPixel(x, y, px);
        }
    }

    return true;
}

std::string Steganography::decodeMessage() const {
    const std::size_t totalBits = capacityBits(m_image);
    if (totalBits < 32) {
        throw std::runtime_error("Image too small for steganography header");
    }

    std::vector<std::uint8_t> bits;
    bits.reserve(totalBits);

    for (int y = 0; y < m_image.height(); ++y) {
        for (int x = 0; x < m_image.width(); ++x) {
            const Color& px = m_image.getPixel(x, y);
            bits.push_back(getChannelLSB(px.r));
            bits.push_back(getChannelLSB(px.g));
            bits.push_back(getChannelLSB(px.b));
        }
    }

    std::uint32_t messageLen = 0;
    for (int i = 0; i < 32; ++i) {
        messageLen = static_cast<std::uint32_t>((messageLen << 1) | bits[static_cast<std::size_t>(i)]);
    }

    const std::size_t neededBits = 32 + static_cast<std::size_t>(messageLen) * 8;
    if (neededBits > bits.size()) {
        throw std::runtime_error("Invalid steganography payload length");
    }

    std::string message;
    message.resize(messageLen);

    std::size_t bitPos = 32;
    for (std::uint32_t i = 0; i < messageLen; ++i) {
        std::uint8_t value = 0;
        for (int b = 0; b < 8; ++b) {
            value = static_cast<std::uint8_t>((value << 1) | bits[bitPos++]);
        }
        message[static_cast<std::size_t>(i)] = static_cast<char>(value);
    }

    return message;
}
