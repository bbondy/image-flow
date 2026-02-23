#include "example_api.h"
#include "bmp.h"
#include "effects.h"
#include "gif.h"
#include "jpg.h"
#include "layer.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>

namespace {
PNGImage rasterToPNG(const RasterImage& src) {
    PNGImage out(src.width(), src.height(), Color(0, 0, 0));
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            out.setPixel(x, y, src.getPixel(x, y));
        }
    }
    return out;
}
} // namespace

int main() {
    try {
        const std::string outDir = "samples/output";
        const std::string samplesDir = "samples";
        std::filesystem::create_directories(outDir);
        std::filesystem::create_directories(samplesDir);
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
        PNGImage resizedUpBoxAverage = resizeImage(smileyPng, 512, 512, ResizeFilter::BoxAverage);
        if (!resizedUpBoxAverage.save(outDir + "/smiley_resize_512_box_average.png")) {
            std::cerr << "Failed to write smiley_resize_512_box_average.png\n";
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

        const std::string tahoeInputWebp = samplesDir + "/tahoe200-finish.webp";
        if (!std::filesystem::exists(tahoeInputWebp)) {
            std::cout << "Skipping Tahoe effect samples (missing " << tahoeInputWebp << ")\n";
        } else if (!WEBPImage::isToolingAvailable()) {
            std::cout << "Skipping Tahoe effect samples (install cwebp and dwebp to enable WebP decode)\n";
        } else {
            WEBPImage tahoeWebp = WEBPImage::load(tahoeInputWebp);

            PNGImage tahoeOriginal = rasterToPNG(tahoeWebp);
            if (!tahoeOriginal.save(outDir + "/tahoe200-original.png")) {
                std::cerr << "Failed to write tahoe200-original.png\n";
                return 1;
            }

            WEBPImage tahoeGrayWebp = tahoeWebp;
            applyGrayscale(tahoeGrayWebp);
            PNGImage tahoeGray = rasterToPNG(tahoeGrayWebp);
            if (!tahoeGray.save(outDir + "/tahoe200-grayscale.png")) {
                std::cerr << "Failed to write tahoe200-grayscale.png\n";
                return 1;
            }

            WEBPImage tahoeSepiaWebp = tahoeWebp;
            applySepia(tahoeSepiaWebp, 1.0f);
            PNGImage tahoeSepia = rasterToPNG(tahoeSepiaWebp);
            if (!tahoeSepia.save(outDir + "/tahoe200-sepia.png")) {
                std::cerr << "Failed to write tahoe200-sepia.png\n";
                return 1;
            }

            Document doc(tahoeWebp.width(), tahoeWebp.height());
            Layer layer("Tahoe Layer", tahoeWebp.width(), tahoeWebp.height(), PixelRGBA8(0, 0, 0, 0));
            layer.image() = fromRasterImage(tahoeWebp, 255);
            applySepia(layer, 0.65f);
            doc.addLayer(layer);

            const ImageBuffer layeredSepiaBuffer = doc.composite();
            PNGImage tahoeSepiaLayered(tahoeWebp.width(), tahoeWebp.height(), Color(0, 0, 0));
            copyToRasterImage(layeredSepiaBuffer, tahoeSepiaLayered);
            if (!tahoeSepiaLayered.save(outDir + "/tahoe200-sepia-layered.png")) {
                std::cerr << "Failed to write tahoe200-sepia-layered.png\n";
                return 1;
            }
        }

        std::cout << "Wrote smiley.bmp, smiley.png, smiley.jpg, smiley.gif, "
                     "smiley.svg, smiley_svg_rasterized_512.png, smiley_copy.bmp, smiley_copy.png, smiley_copy.jpg, smiley_copy.gif, "
                     "smiley_copy.svg, layered_blend.png, smiley_resize_128.png, smiley_resize_512.png, smiley_resize_512_nearest.png, "
                     "smiley_resize_512_box_average.png, "
                     "smiley_direct.png, smiley_layered.png, smiley_layer_diff.png, "
                     "samples/output/tahoe200-original.png, samples/output/tahoe200-grayscale.png, "
                     "samples/output/tahoe200-sepia.png, and samples/output/tahoe200-sepia-layered.png ("
                  << bmpDecoded.width() << "x" << bmpDecoded.height() << ")\n";
        std::cout << "Layered vs direct smiley diff: mean=" << meanDiff << " max=" << maxDiff << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
