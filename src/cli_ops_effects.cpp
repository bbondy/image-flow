#include "cli_ops_effects.h"
#include "cli_ops_resolve.h"

#include "cli_parse.h"
#include "cli_shared.h"

#include "effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

std::uint8_t clampByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}

PixelRGBA8 lerpPixel(const PixelRGBA8& a, const PixelRGBA8& b, float t) {
    const float clamped = clamp01(t);
    const float inv = 1.0f - clamped;
    return PixelRGBA8(
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.r) + clamped * static_cast<float>(b.r)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.g) + clamped * static_cast<float>(b.g)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.b) + clamped * static_cast<float>(b.b)))),
        clampByte(static_cast<int>(std::lround(inv * static_cast<float>(a.a) + clamped * static_cast<float>(b.a)))));
}

double rgbDistance(const PixelRGBA8& a, const PixelRGBA8& b) {
    const double dr = static_cast<double>(a.r) - static_cast<double>(b.r);
    const double dg = static_cast<double>(a.g) - static_cast<double>(b.g);
    const double db = static_cast<double>(a.b) - static_cast<double>(b.b);
    return std::sqrt((dr * dr) + (dg * dg) + (db * db));
}

void applyReplaceColorToLayer(Layer& layer,
                              const PixelRGBA8& fromColor,
                              const PixelRGBA8& toColor,
                              double tolerance,
                              double softness,
                              bool preserveLuma) {
    const double clampedTolerance = std::max(0.0, tolerance);
    const double clampedSoftness = std::max(0.0, softness);
    const double hard = clampedTolerance;
    const double softEnd = clampedTolerance + clampedSoftness;

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const double dist = rgbDistance(src, fromColor);

            float mix = 0.0f;
            if (dist <= hard) {
                mix = 1.0f;
            } else if (softEnd > hard && dist < softEnd) {
                mix = static_cast<float>(1.0 - ((dist - hard) / (softEnd - hard)));
            }

            if (mix <= 0.0f) {
                continue;
            }

            PixelRGBA8 adjusted = toColor;
            adjusted.a = src.a;
            if (preserveLuma) {
                const float srcLuma = 0.299f * static_cast<float>(src.r) +
                                      0.587f * static_cast<float>(src.g) +
                                      0.114f * static_cast<float>(src.b);
                const float dstLuma = 0.299f * static_cast<float>(adjusted.r) +
                                      0.587f * static_cast<float>(adjusted.g) +
                                      0.114f * static_cast<float>(adjusted.b);
                if (dstLuma > 0.0f) {
                    const float scale = srcLuma / dstLuma;
                    adjusted.r = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.r))));
                    adjusted.g = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.g))));
                    adjusted.b = clampByte(static_cast<int>(std::lround(scale * static_cast<float>(adjusted.b))));
                }
            }

            image.setPixel(x, y, lerpPixel(src, adjusted, mix));
        }
    }
}

void applyChannelMixToLayer(Layer& layer,
                            const std::array<float, 9>& mixMatrix,
                            float clampMin,
                            float clampMax) {
    const float minV = std::min(clampMin, clampMax);
    const float maxV = std::max(clampMin, clampMax);

    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float r = static_cast<float>(src.r);
            const float g = static_cast<float>(src.g);
            const float b = static_cast<float>(src.b);

            float outR = mixMatrix[0] * r + mixMatrix[1] * g + mixMatrix[2] * b;
            float outG = mixMatrix[3] * r + mixMatrix[4] * g + mixMatrix[5] * b;
            float outB = mixMatrix[6] * r + mixMatrix[7] * g + mixMatrix[8] * b;

            outR = std::max(minV, std::min(maxV, outR));
            outG = std::max(minV, std::min(maxV, outG));
            outB = std::max(minV, std::min(maxV, outB));

            image.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(outR))),
                                            clampByte(static_cast<int>(std::lround(outG))),
                                            clampByte(static_cast<int>(std::lround(outB))),
                                            src.a));
        }
    }
}

void applyInvertToLayer(Layer& layer, bool preserveAlpha) {
    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            image.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(255 - src.r),
                                            static_cast<std::uint8_t>(255 - src.g),
                                            static_cast<std::uint8_t>(255 - src.b),
                                            preserveAlpha ? src.a : static_cast<std::uint8_t>(255 - src.a)));
        }
    }
}

void applyThresholdToLayer(Layer& layer, int threshold, const PixelRGBA8& lo, const PixelRGBA8& hi) {
    const int t = std::max(0, std::min(255, threshold));
    ImageBuffer& image = layer.image();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const int luma = static_cast<int>(std::lround(0.299 * static_cast<double>(src.r) +
                                                          0.587 * static_cast<double>(src.g) +
                                                          0.114 * static_cast<double>(src.b)));
            image.setPixel(x, y, luma >= t ? hi : lo);
        }
    }
}

float luma01(const PixelRGBA8& p) {
    return (0.299f * static_cast<float>(p.r) +
            0.587f * static_cast<float>(p.g) +
            0.114f * static_cast<float>(p.b)) / 255.0f;
}

PixelRGBA8 sampleClamped(const ImageBuffer& image, int x, int y) {
    const int sx = std::max(0, std::min(image.width() - 1, x));
    const int sy = std::max(0, std::min(image.height() - 1, y));
    return image.getPixel(sx, sy);
}

void applyGaussianBlurToBuffer(ImageBuffer& image, int radius, double sigma) {
    if (radius <= 0) {
        return;
    }
    const double effectiveSigma = sigma > 0.0 ? sigma : (0.3 * static_cast<double>(radius) + 0.8);

    std::vector<float> kernel(static_cast<std::size_t>(radius * 2 + 1), 0.0f);
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double x = static_cast<double>(i);
        const double w = std::exp(-(x * x) / (2.0 * effectiveSigma * effectiveSigma));
        kernel[static_cast<std::size_t>(i + radius)] = static_cast<float>(w);
        sum += w;
    }
    for (float& w : kernel) {
        w = static_cast<float>(w / sum);
    }

    ImageBuffer tmp(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                const PixelRGBA8 s = sampleClamped(image, x + k, y);
                const float w = kernel[static_cast<std::size_t>(k + radius)];
                ar += w * static_cast<double>(s.r);
                ag += w * static_cast<double>(s.g);
                ab += w * static_cast<double>(s.b);
                aa += w * static_cast<double>(s.a);
            }
            tmp.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(ar))),
                                          clampByte(static_cast<int>(std::lround(ag))),
                                          clampByte(static_cast<int>(std::lround(ab))),
                                          clampByte(static_cast<int>(std::lround(aa)))));
        }
    }

    ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                const PixelRGBA8 s = sampleClamped(tmp, x, y + k);
                const float w = kernel[static_cast<std::size_t>(k + radius)];
                ar += w * static_cast<double>(s.r);
                ag += w * static_cast<double>(s.g);
                ab += w * static_cast<double>(s.b);
                aa += w * static_cast<double>(s.a);
            }
            out.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(std::lround(ar))),
                                          clampByte(static_cast<int>(std::lround(ag))),
                                          clampByte(static_cast<int>(std::lround(ab))),
                                          clampByte(static_cast<int>(std::lround(aa)))));
        }
    }
    image = out;
}

void applySobelToBuffer(ImageBuffer& image, bool keepAlpha) {
    static const int kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}};
    static const int ky[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}};

    ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 255));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const float l = luma01(sampleClamped(image, x + i, y + j));
                    gx += static_cast<double>(kx[j + 1][i + 1]) * l;
                    gy += static_cast<double>(ky[j + 1][i + 1]) * l;
                }
            }
            const double mag = std::sqrt(gx * gx + gy * gy);
            const int m = std::max(0, std::min(255, static_cast<int>(std::lround(255.0 * std::min(1.0, mag / 4.0)))));
            const std::uint8_t alpha = keepAlpha ? image.getPixel(x, y).a : 255;
            out.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(m),
                                          static_cast<std::uint8_t>(m),
                                          static_cast<std::uint8_t>(m),
                                          alpha));
        }
    }
    image = out;
}

void applyCannyToBuffer(ImageBuffer& image, int lowThreshold, int highThreshold, bool keepAlpha) {
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) {
        return;
    }

    std::vector<float> gx(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> gy(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> mag(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    std::vector<float> dir(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    auto idx = [w](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x); };

    static const int kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}};
    static const int ky[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sx = 0.0f;
            float sy = 0.0f;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const float l = luma01(sampleClamped(image, x + i, y + j));
                    sx += static_cast<float>(kx[j + 1][i + 1]) * l;
                    sy += static_cast<float>(ky[j + 1][i + 1]) * l;
                }
            }
            gx[idx(x, y)] = sx;
            gy[idx(x, y)] = sy;
            mag[idx(x, y)] = std::sqrt(sx * sx + sy * sy);
            dir[idx(x, y)] = std::atan2(sy, sx);
        }
    }

    std::vector<float> nms(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
    for (int y = 1; y + 1 < h; ++y) {
        for (int x = 1; x + 1 < w; ++x) {
            const float angle = dir[idx(x, y)] * 180.0f / 3.14159265358979323846f;
            float norm = angle;
            if (norm < 0.0f) {
                norm += 180.0f;
            }

            float q = 0.0f;
            float r = 0.0f;
            if ((norm >= 0.0f && norm < 22.5f) || (norm >= 157.5f && norm <= 180.0f)) {
                q = mag[idx(x + 1, y)];
                r = mag[idx(x - 1, y)];
            } else if (norm >= 22.5f && norm < 67.5f) {
                q = mag[idx(x + 1, y - 1)];
                r = mag[idx(x - 1, y + 1)];
            } else if (norm >= 67.5f && norm < 112.5f) {
                q = mag[idx(x, y + 1)];
                r = mag[idx(x, y - 1)];
            } else {
                q = mag[idx(x - 1, y - 1)];
                r = mag[idx(x + 1, y + 1)];
            }

            const float m = mag[idx(x, y)];
            nms[idx(x, y)] = (m >= q && m >= r) ? m : 0.0f;
        }
    }

    const float low = static_cast<float>(std::max(0, std::min(255, lowThreshold))) / 255.0f;
    const float high = static_cast<float>(std::max(0, std::min(255, highThreshold))) / 255.0f;

    std::vector<std::uint8_t> edges(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    std::deque<std::pair<int, int>> q;
    for (int y = 1; y + 1 < h; ++y) {
        for (int x = 1; x + 1 < w; ++x) {
            const float m = nms[idx(x, y)];
            if (m >= high) {
                edges[idx(x, y)] = 255;
                q.emplace_back(x, y);
            } else if (m >= low) {
                edges[idx(x, y)] = 128;
            }
        }
    }

    while (!q.empty()) {
        const auto [x, y] = q.front();
        q.pop_front();
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                if (i == 0 && j == 0) {
                    continue;
                }
                const int nx = x + i;
                const int ny = y + j;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                    continue;
                }
                std::uint8_t& e = edges[idx(nx, ny)];
                if (e == 128) {
                    e = 255;
                    q.emplace_back(nx, ny);
                }
            }
        }
    }

    ImageBuffer out(w, h, PixelRGBA8(0, 0, 0, 255));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t v = edges[idx(x, y)] == 255 ? 255 : 0;
            const std::uint8_t alpha = keepAlpha ? image.getPixel(x, y).a : 255;
            out.setPixel(x, y, PixelRGBA8(v, v, v, alpha));
        }
    }
    image = out;
}

void applyMorphologyToBuffer(ImageBuffer& image, const std::string& op, int radius, int iterations) {
    if (radius <= 0 || iterations <= 0) {
        return;
    }
    const bool dilate = op == "dilate";
    const bool erode = op == "erode";
    if (!dilate && !erode) {
        throw std::runtime_error("morphology op must be erode or dilate");
    }

    for (int iter = 0; iter < iterations; ++iter) {
        ImageBuffer out(image.width(), image.height(), PixelRGBA8(0, 0, 0, 0));
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                int bestR = dilate ? 0 : 255;
                int bestG = dilate ? 0 : 255;
                int bestB = dilate ? 0 : 255;
                int bestA = dilate ? 0 : 255;
                for (int j = -radius; j <= radius; ++j) {
                    for (int i = -radius; i <= radius; ++i) {
                        if ((i * i + j * j) > (radius * radius)) {
                            continue;
                        }
                        const PixelRGBA8 s = sampleClamped(image, x + i, y + j);
                        if (dilate) {
                            bestR = std::max(bestR, static_cast<int>(s.r));
                            bestG = std::max(bestG, static_cast<int>(s.g));
                            bestB = std::max(bestB, static_cast<int>(s.b));
                            bestA = std::max(bestA, static_cast<int>(s.a));
                        } else {
                            bestR = std::min(bestR, static_cast<int>(s.r));
                            bestG = std::min(bestG, static_cast<int>(s.g));
                            bestB = std::min(bestB, static_cast<int>(s.b));
                            bestA = std::min(bestA, static_cast<int>(s.a));
                        }
                    }
                }
                out.setPixel(x, y, PixelRGBA8(static_cast<std::uint8_t>(bestR),
                                              static_cast<std::uint8_t>(bestG),
                                              static_cast<std::uint8_t>(bestB),
                                              static_cast<std::uint8_t>(bestA)));
            }
        }
        image = out;
    }
}

void applyGammaToBuffer(ImageBuffer& image, double gamma) {
    if (gamma <= 0.0) {
        throw std::runtime_error("gamma must be > 0");
    }
    const double invGamma = 1.0 / gamma;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const auto map = [invGamma](std::uint8_t v) {
                const double n = static_cast<double>(v) / 255.0;
                return clampByte(static_cast<int>(std::lround(255.0 * std::pow(n, invGamma))));
            };
            image.setPixel(x, y, PixelRGBA8(map(src.r), map(src.g), map(src.b), src.a));
        }
    }
}

void applyLevelsToBuffer(ImageBuffer& image,
                         int inBlack,
                         int inWhite,
                         double midGamma,
                         int outBlack,
                         int outWhite) {
    const double inB = static_cast<double>(std::max(0, std::min(255, inBlack)));
    const double inW = static_cast<double>(std::max(0, std::min(255, inWhite)));
    if (inW <= inB) {
        throw std::runtime_error("levels requires in_white > in_black");
    }
    if (midGamma <= 0.0) {
        throw std::runtime_error("levels gamma must be > 0");
    }
    const double outB = static_cast<double>(std::max(0, std::min(255, outBlack)));
    const double outW = static_cast<double>(std::max(0, std::min(255, outWhite)));

    auto mapLevel = [&](std::uint8_t v) -> std::uint8_t {
        double t = (static_cast<double>(v) - inB) / (inW - inB);
        t = std::max(0.0, std::min(1.0, t));
        t = std::pow(t, 1.0 / midGamma);
        const double out = outB + (outW - outB) * t;
        return clampByte(static_cast<int>(std::lround(out)));
    };

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            image.setPixel(x, y, PixelRGBA8(mapLevel(src.r), mapLevel(src.g), mapLevel(src.b), src.a));
        }
    }
}

std::vector<std::pair<int, int>> parseCurvePoints(const std::string& text) {
    std::vector<std::pair<int, int>> points;
    const std::vector<std::string> tokens = splitNonEmptyByChar(text, ';');
    for (const std::string& t : tokens) {
        const std::pair<int, int> p = parseIntPair(t);
        points.push_back({std::max(0, std::min(255, p.first)), std::max(0, std::min(255, p.second))});
    }
    if (points.size() < 2) {
        throw std::runtime_error("curve requires at least 2 points");
    }
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return points;
}

std::array<std::uint8_t, 256> buildCurveLut(const std::vector<std::pair<int, int>>& points) {
    std::array<std::uint8_t, 256> lut{};
    std::size_t seg = 0;
    for (int x = 0; x <= 255; ++x) {
        while (seg + 1 < points.size() && x > points[seg + 1].first) {
            ++seg;
        }
        if (seg + 1 >= points.size()) {
            lut[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(points.back().second);
            continue;
        }
        const int x0 = points[seg].first;
        const int y0 = points[seg].second;
        const int x1 = points[seg + 1].first;
        const int y1 = points[seg + 1].second;
        if (x1 == x0) {
            lut[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(y1);
            continue;
        }
        const double t = static_cast<double>(x - x0) / static_cast<double>(x1 - x0);
        const int y = static_cast<int>(std::lround(static_cast<double>(y0) + (static_cast<double>(y1 - y0) * t)));
        lut[static_cast<std::size_t>(x)] = clampByte(y);
    }
    return lut;
}

void applyCurvesToBuffer(ImageBuffer& image,
                         const std::array<std::uint8_t, 256>& rgbLut,
                         const std::array<std::uint8_t, 256>* rLut,
                         const std::array<std::uint8_t, 256>* gLut,
                         const std::array<std::uint8_t, 256>* bLut) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            std::uint8_t r = rgbLut[src.r];
            std::uint8_t g = rgbLut[src.g];
            std::uint8_t b = rgbLut[src.b];
            if (rLut) {
                r = (*rLut)[r];
            }
            if (gLut) {
                g = (*gLut)[g];
            }
            if (bLut) {
                b = (*bLut)[b];
            }
            image.setPixel(x, y, PixelRGBA8(r, g, b, src.a));
        }
    }
}

float hashUnitNoise(int x, int y, std::uint32_t seed) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u;
    n ^= static_cast<std::uint32_t>(y) * 668265263u;
    n ^= seed * 2246822519u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return static_cast<float>(n & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float smoothstep01(float t) {
    const float c = clamp01(t);
    return c * c * (3.0f - 2.0f * c);
}

float valueNoise(float x, float y, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = smoothstep01(x - static_cast<float>(x0));
    const float ty = smoothstep01(y - static_cast<float>(y0));

    const float v00 = hashUnitNoise(x0, y0, seed);
    const float v10 = hashUnitNoise(x1, y0, seed);
    const float v01 = hashUnitNoise(x0, y1, seed);
    const float v11 = hashUnitNoise(x1, y1, seed);

    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

float fractalNoise(float x, float y, int octaves, float lacunarity, float gain, std::uint32_t seed) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        const std::uint32_t octaveSeed = seed + static_cast<std::uint32_t>(o * 1013);
        sum += amplitude * valueNoise(x * frequency, y * frequency, octaveSeed);
        norm += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    if (norm <= 0.0f) {
        return 0.0f;
    }
    return sum / norm;
}

void applyFractalNoiseToBuffer(ImageBuffer& image,
                               float scale,
                               int octaves,
                               float lacunarity,
                               float gain,
                               float amount,
                               std::uint32_t seed,
                               bool monochrome) {
    const float s = scale <= 0.0f ? 64.0f : scale;
    const int oct = std::max(1, octaves);
    const float lac = std::max(1.01f, lacunarity);
    const float g = std::max(0.01f, std::min(1.0f, gain));
    const float mix = clamp01(amount);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float nx = static_cast<float>(x) / s;
            const float ny = static_cast<float>(y) / s;
            const float n = fractalNoise(nx, ny, oct, lac, g, seed);
            const float c = (n * 2.0f) - 1.0f;
            int dr = static_cast<int>(std::lround(c * 255.0f * mix));
            int dg = dr;
            int db = dr;
            if (!monochrome) {
                const float n2 = fractalNoise(nx + 37.2f, ny + 11.7f, oct, lac, g, seed + 97u);
                const float n3 = fractalNoise(nx + 73.9f, ny + 19.3f, oct, lac, g, seed + 211u);
                dg = static_cast<int>(std::lround(((n2 * 2.0f) - 1.0f) * 255.0f * mix));
                db = static_cast<int>(std::lround(((n3 * 2.0f) - 1.0f) * 255.0f * mix));
            }
            image.setPixel(x, y, PixelRGBA8(clampByte(static_cast<int>(src.r) + dr),
                                            clampByte(static_cast<int>(src.g) + dg),
                                            clampByte(static_cast<int>(src.b) + db),
                                            src.a));
        }
    }
}

bool hatchHit(int x, int y, int spacing, int width, int mode) {
    const int m = std::max(1, spacing);
    const int w = std::max(1, width);
    if (mode == 0) { // /
        return ((x + y) % m) < w;
    }
    if (mode == 1) { // backslash diagonal
        return ((x - y + 1000000) % m) < w;
    }
    if (mode == 2) { // horizontal
        return (y % m) < w;
    }
    return (x % m) < w; // vertical
}

void applyHatchToBuffer(ImageBuffer& image,
                        int spacing,
                        int lineWidth,
                        const PixelRGBA8& ink,
                        float opacity,
                        bool preserveHighlights) {
    const float mixBase = clamp01(opacity);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const PixelRGBA8 src = image.getPixel(x, y);
            const float darkness = 1.0f - luma01(src);
            if (darkness <= 0.05f && preserveHighlights) {
                continue;
            }

            bool hit = false;
            if (darkness > 0.18f) hit |= hatchHit(x, y, spacing, lineWidth, 0);
            if (darkness > 0.35f) hit |= hatchHit(x, y, spacing + 2, lineWidth, 1);
            if (darkness > 0.55f) hit |= hatchHit(x, y, spacing + 4, lineWidth, 2);
            if (darkness > 0.75f) hit |= hatchHit(x, y, spacing + 6, lineWidth, 3);
            if (!hit) {
                continue;
            }

            const float mix = clamp01(mixBase * darkness);
            PixelRGBA8 target = ink;
            target.a = src.a;
            image.setPixel(x, y, lerpPixel(src, target, mix));
        }
    }
}

void blendPixelOver(ImageBuffer& image, int x, int y, const PixelRGBA8& color, float alpha) {
    if (!image.inBounds(x, y) || alpha <= 0.0f) {
        return;
    }
    const float a = clamp01(alpha);
    const PixelRGBA8 dst = image.getPixel(x, y);
    image.setPixel(x, y, lerpPixel(dst, PixelRGBA8(color.r, color.g, color.b, dst.a), a));
}

void drawSoftLine(ImageBuffer& image,
                  int x0,
                  int y0,
                  int x1,
                  int y1,
                  const PixelRGBA8& ink,
                  float opacity,
                  int thickness) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int steps = std::max(1, std::max(dx, dy));
    const float invSteps = 1.0f / static_cast<float>(steps);
    const int radius = std::max(0, thickness / 2);

    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) * invSteps;
        const int x = static_cast<int>(std::lround(static_cast<float>(x0) + (static_cast<float>(x1 - x0) * t)));
        const int y = static_cast<int>(std::lround(static_cast<float>(y0) + (static_cast<float>(y1 - y0) * t)));

        for (int oy = -radius; oy <= radius; ++oy) {
            for (int ox = -radius; ox <= radius; ++ox) {
                const float d2 = static_cast<float>(ox * ox + oy * oy);
                const float falloff = radius == 0 ? 1.0f : std::max(0.0f, 1.0f - (d2 / static_cast<float>((radius + 1) * (radius + 1))));
                blendPixelOver(image, x + ox, y + oy, ink, opacity * falloff);
            }
        }
    }
}

void applyPencilStrokesToBuffer(ImageBuffer& image,
                                int spacing,
                                int length,
                                int thickness,
                                double angleDegrees,
                                double angleJitterDegrees,
                                int positionJitter,
                                const PixelRGBA8& ink,
                                float opacity,
                                float minDarkness,
                                std::uint32_t seed) {
    const int step = std::max(1, spacing);
    const int strokeLength = std::max(1, length);
    const int jitter = std::max(0, positionJitter);
    const float minDark = clamp01(minDarkness);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleJitter(-static_cast<float>(angleJitterDegrees),
                                                      static_cast<float>(angleJitterDegrees));
    std::uniform_int_distribution<int> posJitter(-jitter, jitter);

    const double baseRad = angleDegrees * 3.14159265358979323846 / 180.0;
    for (int y = 0; y < image.height(); y += step) {
        for (int x = 0; x < image.width(); x += step) {
            const int sx = x + posJitter(rng);
            const int sy = y + posJitter(rng);
            if (!image.inBounds(sx, sy)) {
                continue;
            }

            const float darkness = 1.0f - luma01(image.getPixel(sx, sy));
            if (darkness < minDark) {
                continue;
            }

            const float spawnChance = clamp01((darkness - minDark) / std::max(0.0001f, 1.0f - minDark));
            if (unit(rng) > spawnChance) {
                continue;
            }

            const double theta = baseRad + (static_cast<double>(angleJitter(rng)) * 3.14159265358979323846 / 180.0);
            const double half = static_cast<double>(strokeLength) * 0.5;
            const int x0 = static_cast<int>(std::lround(static_cast<double>(sx) - std::cos(theta) * half));
            const int y0 = static_cast<int>(std::lround(static_cast<double>(sy) - std::sin(theta) * half));
            const int x1 = static_cast<int>(std::lround(static_cast<double>(sx) + std::cos(theta) * half));
            const int y1 = static_cast<int>(std::lround(static_cast<double>(sy) + std::sin(theta) * half));
            const float strokeOpacity = clamp01(opacity * (0.45f + darkness * 0.9f));
            drawSoftLine(image, x0, y0, x1, y1, ink, strokeOpacity, thickness);
        }
    }
}

bool tryApplyLambdaDispatchedOperation(
    const std::string& action,
    Document& document,
    const std::unordered_map<std::string, std::string>& kv) {
    using OpHandler = std::function<void()>;
    const std::unordered_map<std::string, OpHandler> dispatch = {
        {"apply-effect", [&]() {
             if (kv.find("path") == kv.end() || kv.find("effect") == kv.end()) {
                 throw std::runtime_error("apply-effect requires path= and effect=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const std::string effect = toLower(kv.at("effect"));
             if (effect == "grayscale") {
                 applyGrayscale(layer);
                 return;
             }
             if (effect == "sepia") {
                 const float strength = kv.find("strength") == kv.end() ? 1.0f : std::stof(kv.at("strength"));
                 applySepia(layer, strength);
                 return;
             }
             if (effect == "invert") {
                 const bool preserveAlpha = kv.find("preserve_alpha") == kv.end() ? true : parseBoolFlag(kv.at("preserve_alpha"));
                 applyInvertToLayer(layer, preserveAlpha);
                 return;
             }
             if (effect == "threshold") {
                 const int threshold = kv.find("threshold") == kv.end() ? 128 : std::stoi(kv.at("threshold"));
                 const PixelRGBA8 lo = kv.find("lo") == kv.end() ? PixelRGBA8(0, 0, 0, 255) : parseRGBA(kv.at("lo"), true);
                 const PixelRGBA8 hi = kv.find("hi") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("hi"), true);
                 applyThresholdToLayer(layer, threshold, lo, hi);
                 return;
             }
             throw std::runtime_error("Unsupported effect: " + effect);
         }},
        {"gaussian-blur", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("gaussian-blur requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int radius = kv.find("radius") == kv.end() ? 3 : std::stoi(kv.at("radius"));
             const double sigma = kv.find("sigma") == kv.end() ? 0.0 : std::stod(kv.at("sigma"));
             applyGaussianBlurToBuffer(target, radius, sigma);
         }},
        {"edge-detect", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("edge-detect requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::string method = kv.find("method") == kv.end() ? "sobel" : toLower(kv.at("method"));
             const bool keepAlpha = kv.find("keep_alpha") == kv.end() ? true : parseBoolFlag(kv.at("keep_alpha"));
             if (method == "sobel") {
                 applySobelToBuffer(target, keepAlpha);
                 return;
             }
             if (method == "canny") {
                 const int low = kv.find("low") == kv.end() ? 40 : std::stoi(kv.at("low"));
                 const int high = kv.find("high") == kv.end() ? 90 : std::stoi(kv.at("high"));
                 applyCannyToBuffer(target, low, high, keepAlpha);
                 return;
             }
             throw std::runtime_error("edge-detect method must be sobel or canny");
         }},
        {"morphology", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("morphology requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::string op = kv.find("op") == kv.end() ? "dilate" : toLower(kv.at("op"));
             const int radius = kv.find("radius") == kv.end() ? 1 : std::stoi(kv.at("radius"));
             const int iterations = kv.find("iterations") == kv.end() ? 1 : std::stoi(kv.at("iterations"));
             applyMorphologyToBuffer(target, op, radius, iterations);
         }},
        {"gamma", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("gamma requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const double gamma = kv.find("value") == kv.end() ? (kv.find("gamma") == kv.end() ? 1.0 : std::stod(kv.at("gamma"))) : std::stod(kv.at("value"));
             applyGammaToBuffer(target, gamma);
         }},
        {"levels", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("levels requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int inBlack = kv.find("in_black") == kv.end() ? 0 : std::stoi(kv.at("in_black"));
             const int inWhite = kv.find("in_white") == kv.end() ? 255 : std::stoi(kv.at("in_white"));
             const double midGamma = kv.find("gamma") == kv.end() ? 1.0 : std::stod(kv.at("gamma"));
             const int outBlack = kv.find("out_black") == kv.end() ? 0 : std::stoi(kv.at("out_black"));
             const int outWhite = kv.find("out_white") == kv.end() ? 255 : std::stoi(kv.at("out_white"));
             applyLevelsToBuffer(target, inBlack, inWhite, midGamma, outBlack, outWhite);
         }},
        {"curves", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("curves requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const std::vector<std::pair<int, int>> rgbPoints = kv.find("rgb") == kv.end()
                                                                     ? std::vector<std::pair<int, int>>{{0, 0}, {255, 255}}
                                                                     : parseCurvePoints(kv.at("rgb"));
             const std::array<std::uint8_t, 256> rgbLut = buildCurveLut(rgbPoints);
             std::array<std::uint8_t, 256> rLut{};
             std::array<std::uint8_t, 256> gLut{};
             std::array<std::uint8_t, 256> bLut{};
             const bool hasR = kv.find("r") != kv.end();
             const bool hasG = kv.find("g") != kv.end();
             const bool hasB = kv.find("b") != kv.end();
             if (hasR) {
                 rLut = buildCurveLut(parseCurvePoints(kv.at("r")));
             }
             if (hasG) {
                 gLut = buildCurveLut(parseCurvePoints(kv.at("g")));
             }
             if (hasB) {
                 bLut = buildCurveLut(parseCurvePoints(kv.at("b")));
             }
             applyCurvesToBuffer(target, rgbLut, hasR ? &rLut : nullptr, hasG ? &gLut : nullptr, hasB ? &bLut : nullptr);
         }},
        {"fractal-noise", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("fractal-noise requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const float scale = kv.find("scale") == kv.end() ? 64.0f : std::stof(kv.at("scale"));
             const int octaves = kv.find("octaves") == kv.end() ? 5 : std::stoi(kv.at("octaves"));
             const float lacunarity = kv.find("lacunarity") == kv.end() ? 2.0f : std::stof(kv.at("lacunarity"));
             const float gain = kv.find("gain") == kv.end() ? 0.5f : std::stof(kv.at("gain"));
             const float amount = kv.find("amount") == kv.end() ? 0.2f : std::stof(kv.at("amount"));
             const std::uint32_t seed = kv.find("seed") == kv.end() ? 1337u : static_cast<std::uint32_t>(std::stoul(kv.at("seed")));
             const bool monochrome = kv.find("monochrome") == kv.end() ? true : parseBoolFlag(kv.at("monochrome"));
             applyFractalNoiseToBuffer(target, scale, octaves, lacunarity, gain, amount, seed, monochrome);
         }},
        {"hatch", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("hatch requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int spacing = kv.find("spacing") == kv.end() ? 8 : std::stoi(kv.at("spacing"));
             const int lineWidth = kv.find("line_width") == kv.end() ? 1 : std::stoi(kv.at("line_width"));
             const PixelRGBA8 ink = kv.find("ink") == kv.end() ? PixelRGBA8(28, 28, 28, 255) : parseRGBA(kv.at("ink"), true);
             const float opacity = kv.find("opacity") == kv.end() ? 0.9f : std::stof(kv.at("opacity"));
             const bool preserveHighlights = kv.find("preserve_highlights") == kv.end() ? true : parseBoolFlag(kv.at("preserve_highlights"));
             applyHatchToBuffer(target, spacing, lineWidth, ink, opacity, preserveHighlights);
         }},
        {"pencil-strokes", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("pencil-strokes requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             ImageBuffer& target = resolveDrawTargetBuffer(layer, kv);
             const int spacing = kv.find("spacing") == kv.end() ? 8 : std::stoi(kv.at("spacing"));
             const int length = kv.find("length") == kv.end() ? 14 : std::stoi(kv.at("length"));
             const int thickness = kv.find("thickness") == kv.end() ? 1 : std::stoi(kv.at("thickness"));
             const double angle = kv.find("angle") == kv.end() ? 28.0 : std::stod(kv.at("angle"));
             const double angleJitter = kv.find("angle_jitter") == kv.end() ? 26.0 : std::stod(kv.at("angle_jitter"));
             const int jitter = kv.find("jitter") == kv.end() ? 2 : std::stoi(kv.at("jitter"));
             const PixelRGBA8 ink = kv.find("ink") == kv.end() ? PixelRGBA8(26, 26, 26, 255) : parseRGBA(kv.at("ink"), true);
             const float opacity = kv.find("opacity") == kv.end() ? 0.22f : std::stof(kv.at("opacity"));
             const float minDarkness = kv.find("min_darkness") == kv.end() ? 0.15f : std::stof(kv.at("min_darkness"));
             const std::uint32_t seed = kv.find("seed") == kv.end() ? 1337u : static_cast<std::uint32_t>(std::stoul(kv.at("seed")));
             applyPencilStrokesToBuffer(target, spacing, length, thickness, angle, angleJitter, jitter, ink, opacity, minDarkness, seed);
         }},
        {"replace-color", [&]() {
             if (kv.find("path") == kv.end() || kv.find("from") == kv.end() || kv.find("to") == kv.end()) {
                 throw std::runtime_error("replace-color requires path= from= to=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const PixelRGBA8 fromColor = parseRGBA(kv.at("from"), true);
             const PixelRGBA8 toColor = parseRGBA(kv.at("to"), true);
             const double tolerance = kv.find("tolerance") == kv.end() ? 36.0 : std::stod(kv.at("tolerance"));
             const double softness = kv.find("softness") == kv.end() ? 24.0 : std::stod(kv.at("softness"));
             const bool preserveLuma = kv.find("preserve_luma") == kv.end() ? true : parseBoolFlag(kv.at("preserve_luma"));
             applyReplaceColorToLayer(layer, fromColor, toColor, tolerance, softness, preserveLuma);
         }},
        {"channel-mix", [&]() {
             if (kv.find("path") == kv.end()) {
                 throw std::runtime_error("channel-mix requires path=");
             }
             Layer& layer = resolveLayerPath(document, kv.at("path"));
             const std::array<float, 9> matrix = {
                 kv.find("rr") == kv.end() ? 1.0f : std::stof(kv.at("rr")),
                 kv.find("rg") == kv.end() ? 0.0f : std::stof(kv.at("rg")),
                 kv.find("rb") == kv.end() ? 0.0f : std::stof(kv.at("rb")),
                 kv.find("gr") == kv.end() ? 0.0f : std::stof(kv.at("gr")),
                 kv.find("gg") == kv.end() ? 1.0f : std::stof(kv.at("gg")),
                 kv.find("gb") == kv.end() ? 0.0f : std::stof(kv.at("gb")),
                 kv.find("br") == kv.end() ? 0.0f : std::stof(kv.at("br")),
                 kv.find("bg") == kv.end() ? 0.0f : std::stof(kv.at("bg")),
                 kv.find("bb") == kv.end() ? 1.0f : std::stof(kv.at("bb"))};
             const float clampMin = kv.find("min") == kv.end() ? 0.0f : std::stof(kv.at("min"));
             const float clampMax = kv.find("max") == kv.end() ? 255.0f : std::stof(kv.at("max"));
             applyChannelMixToLayer(layer, matrix, clampMin, clampMax);
         }},
    };

    const auto dispatchIt = dispatch.find(action);
    if (dispatchIt == dispatch.end()) {
        return false;
    }
    dispatchIt->second();
    return true;
}
} // namespace

bool tryApplyEffectsOperation(
    const std::string& action,
    Document& document,
    const std::unordered_map<std::string, std::string>& kv) {
    return tryApplyLambdaDispatchedOperation(action, document, kv);
}
