#include "drawable.h"

#include <algorithm>
#include <cmath>

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
