#include "drawable.h"

#include <algorithm>
#include <cmath>

namespace {
bool normalizeRect(int x, int y, int width, int height, int& left, int& top, int& right, int& bottom) {
    if (width == 0 || height == 0) {
        return false;
    }

    const int x0 = width > 0 ? x : x + width;
    const int x1 = width > 0 ? x + width : x;
    const int y0 = height > 0 ? y : y + height;
    const int y1 = height > 0 ? y + height : y;

    left = x0;
    right = x1 - 1;
    top = y0;
    bottom = y1 - 1;
    return true;
}
} // namespace

Drawable::Drawable(Image& image) : m_image(image) {}

void Drawable::setPixel(int x, int y, const Color& color) {
    m_image.setPixel(x, y, color);
}

const Color& Drawable::getPixel(int x, int y) const {
    return m_image.getPixel(x, y);
}

void Drawable::fill(const Color& color) {
    for (int y = 0; y < m_image.height(); ++y) {
        for (int x = 0; x < m_image.width(); ++x) {
            m_image.setPixel(x, y, color);
        }
    }
}

void Drawable::line(int x0, int y0, int x1, int y1, const Color& color) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        m_image.setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Drawable::rect(int x, int y, int width, int height, const Color& color) {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (!normalizeRect(x, y, width, height, left, top, right, bottom)) {
        return;
    }

    line(left, top, right, top, color);
    line(right, top, right, bottom, color);
    line(right, bottom, left, bottom, color);
    line(left, bottom, left, top, color);
}

void Drawable::fillRect(int x, int y, int width, int height, const Color& color) {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (!normalizeRect(x, y, width, height, left, top, right, bottom)) {
        return;
    }

    for (int py = top; py <= bottom; ++py) {
        for (int px = left; px <= right; ++px) {
            m_image.setPixel(px, py, color);
        }
    }
}

void Drawable::plotCircleOctants(int cx, int cy, int x, int y, const Color& color) {
    m_image.setPixel(cx + x, cy + y, color);
    m_image.setPixel(cx - x, cy + y, color);
    m_image.setPixel(cx + x, cy - y, color);
    m_image.setPixel(cx - x, cy - y, color);
    m_image.setPixel(cx + y, cy + x, color);
    m_image.setPixel(cx - y, cy + x, color);
    m_image.setPixel(cx + y, cy - x, color);
    m_image.setPixel(cx - y, cy - x, color);
}

void Drawable::circle(int cx, int cy, int radius, const Color& color) {
    if (radius < 0) {
        return;
    }

    int x = radius;
    int y = 0;
    int err = 1 - x;

    while (x >= y) {
        plotCircleOctants(cx, cy, x, y, color);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void Drawable::fillCircle(int cx, int cy, int radius, const Color& color) {
    if (radius < 0) {
        return;
    }

    for (int y = -radius; y <= radius; ++y) {
        const int xSpan = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - y * y)));
        for (int x = -xSpan; x <= xSpan; ++x) {
            m_image.setPixel(cx + x, cy + y, color);
        }
    }
}

void Drawable::arc(int cx, int cy, int radius, float startRadians, float endRadians, const Color& color) {
    if (radius <= 0) {
        return;
    }
    if (endRadians < startRadians) {
        std::swap(startRadians, endRadians);
    }

    const float step = 1.0f / static_cast<float>(std::max(4, radius));
    int prevX = static_cast<int>(std::lround(cx + radius * std::cos(startRadians)));
    int prevY = static_cast<int>(std::lround(cy + radius * std::sin(startRadians)));

    for (float t = startRadians + step; t <= endRadians + step * 0.5f; t += step) {
        const float clamped = std::min(t, endRadians);
        const int x = static_cast<int>(std::lround(cx + radius * std::cos(clamped)));
        const int y = static_cast<int>(std::lround(cy + radius * std::sin(clamped)));
        line(prevX, prevY, x, y, color);
        prevX = x;
        prevY = y;
    }
}
