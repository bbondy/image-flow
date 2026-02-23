#include "drawable.h"

#include <algorithm>
#include <cmath>
#include <deque>
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

void pushUniquePoint(std::vector<std::pair<int, int>>& points, int x, int y) {
    if (!points.empty() && points.back().first == x && points.back().second == y) {
        return;
    }
    points.push_back({x, y});
}

std::vector<std::pair<int, int>> buildRoundedRectContour(int left, int top, int right, int bottom, int radius) {
    std::vector<std::pair<int, int>> points;
    const float pi = 3.14159265358979323846f;
    const int steps = std::max(4, radius * 2);
    const int r = std::max(0, radius);

    const int trCx = right - r;
    const int trCy = top + r;
    const int brCx = right - r;
    const int brCy = bottom - r;
    const int blCx = left + r;
    const int blCy = bottom - r;
    const int tlCx = left + r;
    const int tlCy = top + r;

    pushUniquePoint(points, left + r, top);
    pushUniquePoint(points, right - r, top);

    auto appendArc = [&](int cx, int cy, float startAngle, float endAngle) {
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float angle = startAngle + (endAngle - startAngle) * t;
            const int px = static_cast<int>(std::lround(static_cast<float>(cx) + static_cast<float>(r) * std::cos(angle)));
            const int py = static_cast<int>(std::lround(static_cast<float>(cy) + static_cast<float>(r) * std::sin(angle)));
            pushUniquePoint(points, px, py);
        }
    };

    appendArc(trCx, trCy, -pi * 0.5f, 0.0f);
    pushUniquePoint(points, right, bottom - r);
    appendArc(brCx, brCy, 0.0f, pi * 0.5f);
    pushUniquePoint(points, left + r, bottom);
    appendArc(blCx, blCy, pi * 0.5f, pi);
    pushUniquePoint(points, left, top + r);
    appendArc(tlCx, tlCy, pi, pi * 1.5f);
    return points;
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

void Drawable::beginPath() {
    m_path.clear();
}

void Drawable::moveTo(float x, float y) {
    SubPath sub;
    sub.points.push_back({x, y});
    m_path.push_back(sub);
}

void Drawable::lineTo(float x, float y) {
    if (m_path.empty()) {
        moveTo(x, y);
        return;
    }
    if (m_path.back().closed) {
        moveTo(x, y);
        return;
    }
    m_path.back().points.push_back({x, y});
}

void Drawable::quadraticCurveTo(float cx, float cy, float x, float y) {
    if (m_path.empty() || m_path.back().points.empty() || m_path.back().closed) {
        moveTo(x, y);
        return;
    }

    const auto p0 = m_path.back().points.back();
    const float span = std::sqrt((x - p0.first) * (x - p0.first) + (y - p0.second) * (y - p0.second));
    const int steps = std::max(16, static_cast<int>(std::ceil(span)));
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float omt = 1.0f - t;
        const float px = omt * omt * p0.first + 2.0f * omt * t * cx + t * t * x;
        const float py = omt * omt * p0.second + 2.0f * omt * t * cy + t * t * y;
        lineTo(px, py);
    }
}

void Drawable::bezierCurveTo(float cx1, float cy1, float cx2, float cy2, float x, float y) {
    if (m_path.empty() || m_path.back().points.empty() || m_path.back().closed) {
        moveTo(x, y);
        return;
    }

    const auto p0 = m_path.back().points.back();
    const float span = std::sqrt((x - p0.first) * (x - p0.first) + (y - p0.second) * (y - p0.second));
    const int steps = std::max(16, static_cast<int>(std::ceil(span)));
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float omt = 1.0f - t;
        const float px = omt * omt * omt * p0.first +
                         3.0f * omt * omt * t * cx1 +
                         3.0f * omt * t * t * cx2 +
                         t * t * t * x;
        const float py = omt * omt * omt * p0.second +
                         3.0f * omt * omt * t * cy1 +
                         3.0f * omt * t * t * cy2 +
                         t * t * t * y;
        lineTo(px, py);
    }
}

void Drawable::closePath() {
    if (m_path.empty()) {
        return;
    }
    SubPath& sub = m_path.back();
    if (sub.points.size() < 2) {
        sub.closed = true;
        return;
    }
    const auto& first = sub.points.front();
    const auto& last = sub.points.back();
    if (first.first != last.first || first.second != last.second) {
        sub.points.push_back(first);
    }
    sub.closed = true;
}

void Drawable::setLineWidth(int width) {
    m_lineWidth = std::max(1, width);
}

void Drawable::setLineCap(LineCap cap) {
    m_lineCap = cap;
}

void Drawable::setLineJoin(LineJoin join) {
    m_lineJoin = join;
}

void Drawable::setMiterLimit(float limit) {
    m_miterLimit = std::max(1.0f, limit);
}

void Drawable::strokeSegment(float x0, float y0, float x1, float y1, const Color& color) {
    if (m_lineWidth <= 1) {
        line(static_cast<int>(std::lround(x0)),
             static_cast<int>(std::lround(y0)),
             static_cast<int>(std::lround(x1)),
             static_cast<int>(std::lround(y1)),
             color);
        return;
    }

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0001f) {
        fillCircle(static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)), m_lineWidth / 2, color);
        return;
    }

    const float ux = dx / len;
    const float uy = dy / len;
    const float px = -uy;
    const float py = ux;
    const float half = static_cast<float>(m_lineWidth - 1) * 0.5f;

    float sx0 = x0;
    float sy0 = y0;
    float sx1 = x1;
    float sy1 = y1;
    if (m_lineCap == LineCap::Square) {
        sx0 -= ux * half;
        sy0 -= uy * half;
        sx1 += ux * half;
        sy1 += uy * half;
    }

    for (int i = 0; i < m_lineWidth; ++i) {
        const float offset = static_cast<float>(i) - half;
        line(static_cast<int>(std::lround(sx0 + px * offset)),
             static_cast<int>(std::lround(sy0 + py * offset)),
             static_cast<int>(std::lround(sx1 + px * offset)),
             static_cast<int>(std::lround(sy1 + py * offset)),
             color);
    }

    if (m_lineCap == LineCap::Round) {
        fillCircle(static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)), m_lineWidth / 2, color);
        fillCircle(static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), m_lineWidth / 2, color);
    }
}

void Drawable::stroke(const Color& color) {
    for (const SubPath& sub : m_path) {
        if (sub.points.size() < 2) {
            continue;
        }
        for (std::size_t i = 1; i < sub.points.size(); ++i) {
            strokeSegment(sub.points[i - 1].first, sub.points[i - 1].second,
                          sub.points[i].first, sub.points[i].second, color);
        }

        if (m_lineWidth > 1 && sub.points.size() > 2) {
            const std::size_t joinEnd = sub.closed ? sub.points.size() - 1 : sub.points.size() - 2;
            for (std::size_t i = 1; i <= joinEnd; ++i) {
                const auto& p = sub.points[i];
                const int jx = static_cast<int>(std::lround(p.first));
                const int jy = static_cast<int>(std::lround(p.second));
                if (m_lineJoin == LineJoin::Round) {
                    fillCircle(jx, jy, m_lineWidth / 2, color);
                } else if (m_lineJoin == LineJoin::Bevel) {
                    const int side = std::max(1, m_lineWidth / 2);
                    fillRect(jx - side, jy - side, side * 2 + 1, side * 2 + 1, color);
                } else {
                    (void)m_miterLimit;
                }
            }
        }
    }
}

void Drawable::fillPath(const Color& color) {
    for (const SubPath& sub : m_path) {
        if (sub.points.size() < 3) {
            continue;
        }
        std::vector<std::pair<int, int>> polygonPoints;
        polygonPoints.reserve(sub.points.size());
        for (const auto& p : sub.points) {
            polygonPoints.push_back({static_cast<int>(std::lround(p.first)),
                                     static_cast<int>(std::lround(p.second))});
        }
        if (polygonPoints.front() != polygonPoints.back()) {
            polygonPoints.push_back(polygonPoints.front());
        }
        fillPolygon(polygonPoints, color);
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

void Drawable::roundRect(int x, int y, int width, int height, int radius, const Color& color) {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (!normalizeRect(x, y, width, height, left, top, right, bottom)) {
        return;
    }

    const int maxRadius = std::max(0, std::min((right - left + 1) / 2, (bottom - top + 1) / 2));
    const int r = std::max(0, std::min(radius, maxRadius));
    if (r == 0) {
        rect(left, top, right - left + 1, bottom - top + 1, color);
        return;
    }

    const std::vector<std::pair<int, int>> contour = buildRoundedRectContour(left, top, right, bottom, r);
    polygon(contour, color);
}

void Drawable::fillRoundRect(int x, int y, int width, int height, int radius, const Color& color) {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (!normalizeRect(x, y, width, height, left, top, right, bottom)) {
        return;
    }

    const int maxRadius = std::max(0, std::min((right - left + 1) / 2, (bottom - top + 1) / 2));
    const int r = std::max(0, std::min(radius, maxRadius));
    if (r == 0) {
        fillRect(left, top, right - left + 1, bottom - top + 1, color);
        return;
    }

    const std::vector<std::pair<int, int>> contour = buildRoundedRectContour(left, top, right, bottom, r);
    fillPolygon(contour, color);
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

void Drawable::floodFill(int x, int y, const Color& color, int tolerance) {
    if (!m_image.inBounds(x, y)) {
        return;
    }

    const int clampedTolerance = std::max(0, std::min(255, tolerance));
    const Color seed = m_image.getPixel(x, y);
    if (seed.r == color.r && seed.g == color.g && seed.b == color.b) {
        return;
    }

    const auto withinTolerance = [seed, clampedTolerance](const Color& c) {
        const int dr = std::abs(static_cast<int>(c.r) - static_cast<int>(seed.r));
        const int dg = std::abs(static_cast<int>(c.g) - static_cast<int>(seed.g));
        const int db = std::abs(static_cast<int>(c.b) - static_cast<int>(seed.b));
        return std::max(dr, std::max(dg, db)) <= clampedTolerance;
    };

    const int w = m_image.width();
    const int h = m_image.height();
    std::vector<std::uint8_t> visited(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    auto indexOf = [w](int px, int py) {
        return static_cast<std::size_t>(py) * static_cast<std::size_t>(w) + static_cast<std::size_t>(px);
    };

    std::deque<std::pair<int, int>> queue;
    queue.push_back({x, y});
    visited[indexOf(x, y)] = 1;

    while (!queue.empty()) {
        const auto [cx, cy] = queue.front();
        queue.pop_front();

        const Color current = m_image.getPixel(cx, cy);
        if (!withinTolerance(current)) {
            continue;
        }
        m_image.setPixel(cx, cy, color);

        const int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& offset : offsets) {
            const int nx = cx + offset[0];
            const int ny = cy + offset[1];
            if (!m_image.inBounds(nx, ny)) {
                continue;
            }
            const std::size_t idx = indexOf(nx, ny);
            if (visited[idx] != 0) {
                continue;
            }
            visited[idx] = 1;
            queue.push_back({nx, ny});
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

void Drawable::arc(int cx, int cy, int radius, float startRadians, float endRadians, const Color& color, bool counterclockwise) {
    if (radius <= 0) {
        return;
    }

    const float twoPi = 6.28318530717958647692f;
    float sweep = std::fmod(endRadians - startRadians, twoPi);
    if (!counterclockwise && sweep < 0.0f) {
        sweep += twoPi;
    } else if (counterclockwise && sweep > 0.0f) {
        sweep -= twoPi;
    }
    if (std::fabs(sweep) < 1e-6f) {
        return;
    }

    const int steps = std::max(4, static_cast<int>(std::ceil(std::fabs(sweep) * static_cast<float>(radius))));
    int prevX = static_cast<int>(std::lround(cx + radius * std::cos(startRadians)));
    int prevY = static_cast<int>(std::lround(cy + radius * std::sin(startRadians)));

    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float angle = startRadians + sweep * t;
        const int x = static_cast<int>(std::lround(cx + radius * std::cos(angle)));
        const int y = static_cast<int>(std::lround(cy + radius * std::sin(angle)));
        line(prevX, prevY, x, y, color);
        prevX = x;
        prevY = y;
    }
}
