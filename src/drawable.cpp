#include "drawable.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

void Drawable::ellipse(int cx, int cy, int rx, int ry, const Color& color) {
    if (rx < 0 || ry < 0) {
        return;
    }
    if (rx == 0 && ry == 0) {
        m_image.setPixel(cx, cy, color);
        return;
    }
    if (ry == 0) {
        line(cx - rx, cy, cx + rx, cy, color);
        return;
    }
    if (rx == 0) {
        line(cx, cy - ry, cx, cy + ry, color);
        return;
    }

    const float twoPi = 6.28318530717958647692f;
    const int steps = std::max(24, std::max(rx, ry) * 8);
    int prevX = static_cast<int>(std::lround(cx + static_cast<float>(rx)));
    int prevY = cy;
    for (int i = 1; i <= steps; ++i) {
        const float t = twoPi * static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(std::lround(cx + static_cast<float>(rx) * std::cos(t)));
        const int y = static_cast<int>(std::lround(cy + static_cast<float>(ry) * std::sin(t)));
        line(prevX, prevY, x, y, color);
        prevX = x;
        prevY = y;
    }
}

void Drawable::fillEllipse(int cx, int cy, int rx, int ry, const Color& color) {
    if (rx < 0 || ry < 0) {
        return;
    }
    if (rx == 0 && ry == 0) {
        m_image.setPixel(cx, cy, color);
        return;
    }
    if (ry == 0) {
        line(cx - rx, cy, cx + rx, cy, color);
        return;
    }
    if (rx == 0) {
        line(cx, cy - ry, cx, cy + ry, color);
        return;
    }

    for (int dy = -ry; dy <= ry; ++dy) {
        const float t = static_cast<float>(dy) / static_cast<float>(ry);
        const float span = static_cast<float>(rx) * std::sqrt(std::max(0.0f, 1.0f - t * t));
        const int xSpan = static_cast<int>(std::floor(span + 0.5f));
        for (int dx = -xSpan; dx <= xSpan; ++dx) {
            m_image.setPixel(cx + dx, cy + dy, color);
        }
    }
}

void Drawable::polyline(const std::vector<std::pair<int, int>>& points, const Color& color) {
    if (points.size() < 2) {
        return;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        line(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second, color);
    }
}

void Drawable::polygon(const std::vector<std::pair<int, int>>& points, const Color& color) {
    if (points.size() < 2) {
        return;
    }
    polyline(points, color);
    line(points.back().first, points.back().second, points.front().first, points.front().second, color);
}

void Drawable::fillPolygon(const std::vector<std::pair<int, int>>& points, const Color& color) {
    if (points.size() < 3) {
        return;
    }

    int minY = points.front().second;
    int maxY = points.front().second;
    for (const auto& p : points) {
        minY = std::min(minY, p.second);
        maxY = std::max(maxY, p.second);
    }

    for (int y = minY; y <= maxY; ++y) {
        const double scanY = static_cast<double>(y) + 0.5;
        std::vector<double> intersections;
        intersections.reserve(points.size());

        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto& a = points[i];
            const auto& b = points[(i + 1) % points.size()];
            if (a.second == b.second) {
                continue;
            }

            const int edgeMinY = std::min(a.second, b.second);
            const int edgeMaxY = std::max(a.second, b.second);
            if (!(scanY >= static_cast<double>(edgeMinY) && scanY < static_cast<double>(edgeMaxY))) {
                continue;
            }

            const double t = (scanY - static_cast<double>(a.second)) /
                             static_cast<double>(b.second - a.second);
            const double x = static_cast<double>(a.first) + t * static_cast<double>(b.first - a.first);
            intersections.push_back(x);
        }

        std::sort(intersections.begin(), intersections.end());
        for (std::size_t i = 0; i + 1 < intersections.size(); i += 2) {
            const int xStart = static_cast<int>(std::ceil(intersections[i]));
            const int xEnd = static_cast<int>(std::floor(intersections[i + 1]));
            for (int x = xStart; x <= xEnd; ++x) {
                m_image.setPixel(x, y, color);
            }
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
