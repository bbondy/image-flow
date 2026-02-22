#include "api.h"
#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"
#include "steganography.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>

int main() {
    try {
        const std::string hidden = "Hello world";
        auto absDiff = [](int a, int b) { return a > b ? (a - b) : (b - a); };

        auto verifyStego = [&](const char* label, RasterImage& image) {
            Steganography stego(image);
            if (!stego.encodeMessage(hidden)) {
                std::cerr << "Failed to encode steganography message into " << label << "\n";
                return false;
            }
            const std::string extracted = stego.decodeMessage();
            if (extracted != hidden) {
                std::cerr << "Steganography decode mismatch for " << label << "\n";
                return false;
            }
            return true;
        };

        BMPImage smileyBmp = api::createSmiley256BMP();
        if (!verifyStego("BMP image object", smileyBmp)) {
            return 1;
        }
        if (!smileyBmp.save("smiley.bmp")) {
            std::cerr << "Failed to write smiley.bmp\n";
            return 1;
        }

        PNGImage smileyPng = api::createSmiley256PNG();
        if (!verifyStego("PNG image object", smileyPng)) {
            return 1;
        }
        if (!smileyPng.save("smiley.png")) {
            std::cerr << "Failed to write smiley.png\n";
            return 1;
        }

        JPGImage smileyJpg = api::createSmiley256JPG();
        if (!verifyStego("JPG image object", smileyJpg)) {
            return 1;
        }
        if (!smileyJpg.save("smiley.jpg")) {
            std::cerr << "Failed to write smiley.jpg\n";
            return 1;
        }

        GIFImage smileyGif = api::createSmiley256GIF();
        if (!verifyStego("GIF image object", smileyGif)) {
            return 1;
        }
        if (!smileyGif.save("smiley.gif")) {
            std::cerr << "Failed to write smiley.gif\n";
            return 1;
        }

        PNGImage layeredBlend = api::createLayerBlendDemoPNG();
        if (!layeredBlend.save("layered_blend.png")) {
            std::cerr << "Failed to write layered_blend.png\n";
            return 1;
        }

        PNGImage directSmiley = api::createSmiley256PNG();
        PNGImage layeredSmiley = api::createSmiley256LayeredPNG();
        if (!directSmiley.save("smiley_direct.png")) {
            std::cerr << "Failed to write smiley_direct.png\n";
            return 1;
        }
        if (!layeredSmiley.save("smiley_layered.png")) {
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
        if (!diff.save("smiley_layer_diff.png")) {
            std::cerr << "Failed to write smiley_layer_diff.png\n";
            return 1;
        }
        const double meanDiff = static_cast<double>(sumDiff) / static_cast<double>(256 * 256 * 3);

        BMPImage bmpDecoded = BMPImage::load("smiley.bmp");
        if (!bmpDecoded.save("smiley_copy.bmp")) {
            std::cerr << "Failed to write smiley_copy.bmp\n";
            return 1;
        }

        PNGImage pngDecoded = PNGImage::load("smiley.png");
        if (!pngDecoded.save("smiley_copy.png")) {
            std::cerr << "Failed to write smiley_copy.png\n";
            return 1;
        }

        JPGImage jpgDecoded = JPGImage::load("smiley.jpg");
        if (!jpgDecoded.save("smiley_copy.jpg")) {
            std::cerr << "Failed to write smiley_copy.jpg\n";
            return 1;
        }

        GIFImage gifDecoded = GIFImage::load("smiley.gif");
        if (!gifDecoded.save("smiley_copy.gif")) {
            std::cerr << "Failed to write smiley_copy.gif\n";
            return 1;
        }

        PNGImage stegoImage = api::createSmiley256PNG();
        Steganography stego(stegoImage);
        if (!stego.encodeMessage(hidden)) {
            std::cerr << "Failed to encode steganography message into persisted PNG\n";
            return 1;
        }
        if (!stegoImage.save("smiley_stego.png")) {
            std::cerr << "Failed to write smiley_stego.png\n";
            return 1;
        }

        PNGImage stegoLoaded = PNGImage::load("smiley_stego.png");
        Steganography stegoReader(stegoLoaded);
        const std::string extracted = stegoReader.decodeMessage();

        if (extracted != hidden) {
            std::cerr << "Steganography decode mismatch\n";
            return 1;
        }

        std::cout << "Wrote smiley.bmp, smiley.png, smiley.jpg, smiley.gif, "
                     "smiley_copy.bmp, smiley_copy.png, smiley_copy.jpg, smiley_copy.gif, smiley_stego.png, layered_blend.png, "
                     "smiley_direct.png, smiley_layered.png, and smiley_layer_diff.png ("
                  << bmpDecoded.width() << "x" << bmpDecoded.height() << ")\n";
        std::cout << "Steganography extracted message: " << extracted << "\n";
        std::cout << "Steganography verified on BMP/PNG/JPG/GIF image objects\n";
        std::cout << "Layered vs direct smiley diff: mean=" << meanDiff << " max=" << maxDiff << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
