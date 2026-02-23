#include "example_api.h"
#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>

int main() {
    try {
        const std::string outDir = "build/output/images";
        std::filesystem::create_directories(outDir);
        auto absDiff = [](int a, int b) { return a > b ? (a - b) : (b - a); };

        BMPImage smileyBmp = example_api::createSmiley256BMP();
        if (!smileyBmp.save(outDir + "/smiley.bmp")) {
            std::cerr << "Failed to write smiley.bmp\n";
            return 1;
        }

        PNGImage smileyPng = example_api::createSmiley256PNG();
        if (!smileyPng.save(outDir + "/smiley.png")) {
            std::cerr << "Failed to write smiley.png\n";
            return 1;
        }

        JPGImage smileyJpg = example_api::createSmiley256JPG();
        if (!smileyJpg.save(outDir + "/smiley.jpg")) {
            std::cerr << "Failed to write smiley.jpg\n";
            return 1;
        }

        GIFImage smileyGif = example_api::createSmiley256GIF();
        if (!smileyGif.save(outDir + "/smiley.gif")) {
            std::cerr << "Failed to write smiley.gif\n";
            return 1;
        }

        const bool hasWebP = WEBPImage::isToolingAvailable();
        if (hasWebP) {
            WEBPImage smileyWebp = example_api::createSmiley256WEBP();
            if (!smileyWebp.save(outDir + "/smiley.webp")) {
                std::cerr << "Failed to write smiley.webp\n";
                return 1;
            }
        } else {
            std::cout << "Skipping WebP output (install cwebp and dwebp to enable)\n";
        }

        SVGImage smileySvg = example_api::createSmiley256SVG();
        if (!smileySvg.save(outDir + "/smiley.svg")) {
            std::cerr << "Failed to write smiley.svg\n";
            return 1;
        }
        PNGImage svgRasterizedPng(512, 512, Color(255, 255, 255));
        rasterizeSVGFileToRaster(outDir + "/smiley.svg", svgRasterizedPng);
        if (!svgRasterizedPng.save(outDir + "/smiley_svg_rasterized_512.png")) {
            std::cerr << "Failed to write smiley_svg_rasterized_512.png\n";
            return 1;
        }

        PNGImage layeredBlend = example_api::createLayerBlendDemoPNG();
        if (!layeredBlend.save(outDir + "/layered_blend.png")) {
            std::cerr << "Failed to write layered_blend.png\n";
            return 1;
        }

        PNGImage resizedDown = resizeImage(smileyPng, 128, 128);
        if (!resizedDown.save(outDir + "/smiley_resize_128.png")) {
            std::cerr << "Failed to write smiley_resize_128.png\n";
            return 1;
        }
        PNGImage resizedUp = resizeImage(smileyPng, 512, 512);
        if (!resizedUp.save(outDir + "/smiley_resize_512.png")) {
            std::cerr << "Failed to write smiley_resize_512.png\n";
            return 1;
        }
        PNGImage resizedUpNearest = resizeImage(smileyPng, 512, 512, ResizeFilter::Nearest);
        if (!resizedUpNearest.save(outDir + "/smiley_resize_512_nearest.png")) {
            std::cerr << "Failed to write smiley_resize_512_nearest.png\n";
            return 1;
        }

        PNGImage directSmiley = example_api::createSmiley256PNG();
        PNGImage layeredSmiley = example_api::createSmiley256LayeredPNG();
        if (!directSmiley.save(outDir + "/smiley_direct.png")) {
            std::cerr << "Failed to write smiley_direct.png\n";
            return 1;
        }
        if (!layeredSmiley.save(outDir + "/smiley_layered.png")) {
            std::cerr << "Failed to write smiley_layered.png\n";
            return 1;
        }

        PNGImage diff(256, 256, Color(0, 0, 0));
        std::uint64_t sumDiff = 0;
        int maxDiff = 0;
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                const Color& a = directSmiley.getPixel(x, y);
                const Color& b = layeredSmiley.getPixel(x, y);
                const int dr = absDiff(static_cast<int>(a.r), static_cast<int>(b.r));
                const int dg = absDiff(static_cast<int>(a.g), static_cast<int>(b.g));
                const int db = absDiff(static_cast<int>(a.b), static_cast<int>(b.b));
                sumDiff += static_cast<std::uint64_t>(dr + dg + db);
                maxDiff = std::max(maxDiff, std::max(dr, std::max(dg, db)));
                diff.setPixel(x, y, Color(static_cast<std::uint8_t>(dr),
                                          static_cast<std::uint8_t>(dg),
                                          static_cast<std::uint8_t>(db)));
            }
        }
        if (!diff.save(outDir + "/smiley_layer_diff.png")) {
            std::cerr << "Failed to write smiley_layer_diff.png\n";
            return 1;
        }
        const double meanDiff = static_cast<double>(sumDiff) / static_cast<double>(256 * 256 * 3);

        BMPImage bmpDecoded = BMPImage::load(outDir + "/smiley.bmp");
        if (!bmpDecoded.save(outDir + "/smiley_copy.bmp")) {
            std::cerr << "Failed to write smiley_copy.bmp\n";
            return 1;
        }

        PNGImage pngDecoded = PNGImage::load(outDir + "/smiley.png");
        if (!pngDecoded.save(outDir + "/smiley_copy.png")) {
            std::cerr << "Failed to write smiley_copy.png\n";
            return 1;
        }

        JPGImage jpgDecoded = JPGImage::load(outDir + "/smiley.jpg");
        if (!jpgDecoded.save(outDir + "/smiley_copy.jpg")) {
            std::cerr << "Failed to write smiley_copy.jpg\n";
            return 1;
        }

        GIFImage gifDecoded = GIFImage::load(outDir + "/smiley.gif");
        if (!gifDecoded.save(outDir + "/smiley_copy.gif")) {
            std::cerr << "Failed to write smiley_copy.gif\n";
            return 1;
        }

        if (hasWebP) {
            WEBPImage webpDecoded = WEBPImage::load(outDir + "/smiley.webp");
            if (!webpDecoded.save(outDir + "/smiley_copy.webp")) {
                std::cerr << "Failed to write smiley_copy.webp\n";
                return 1;
            }
        }

        SVGImage svgDecoded = SVGImage::load(outDir + "/smiley.svg");
        if (!svgDecoded.save(outDir + "/smiley_copy.svg")) {
            std::cerr << "Failed to write smiley_copy.svg\n";
            return 1;
        }

        std::cout << "Wrote smiley.bmp, smiley.png, smiley.jpg, smiley.gif, "
                     "smiley.svg, smiley_svg_rasterized_512.png, smiley_copy.bmp, smiley_copy.png, smiley_copy.jpg, smiley_copy.gif, "
                     "smiley_copy.svg, layered_blend.png, smiley_resize_128.png, smiley_resize_512.png, smiley_resize_512_nearest.png, "
                     "smiley_direct.png, smiley_layered.png, and smiley_layer_diff.png ("
                  << bmpDecoded.width() << "x" << bmpDecoded.height() << ")\n";
        std::cout << "Layered vs direct smiley diff: mean=" << meanDiff << " max=" << maxDiff << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
