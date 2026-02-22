#include "api.h"
#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
struct DiffStats {
    double meanAbs = 0.0;
    int maxAbs = 0;
    std::size_t pixels = 0;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

DiffStats compareImages(const RasterImage& a, const RasterImage& b) {
    require(a.width() == b.width(), "Image width mismatch");
    require(a.height() == b.height(), "Image height mismatch");

    DiffStats stats{};
    stats.pixels = static_cast<std::size_t>(a.width()) * static_cast<std::size_t>(a.height());

    std::uint64_t sum = 0;
    int maxAbs = 0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            const Color& ca = a.getPixel(x, y);
            const Color& cb = b.getPixel(x, y);

            const int dr = std::abs(static_cast<int>(ca.r) - static_cast<int>(cb.r));
            const int dg = std::abs(static_cast<int>(ca.g) - static_cast<int>(cb.g));
            const int db = std::abs(static_cast<int>(ca.b) - static_cast<int>(cb.b));

            sum += static_cast<std::uint64_t>(dr + dg + db);
            maxAbs = std::max(maxAbs, std::max(dr, std::max(dg, db)));
        }
    }

    stats.meanAbs = static_cast<double>(sum) / static_cast<double>(stats.pixels * 3);
    stats.maxAbs = maxAbs;
    return stats;
}

void testReferenceSmileyShape() {
    PNGImage ref = api::createSmiley256PNG();

    require(ref.width() == 256 && ref.height() == 256, "Reference smiley dimensions must be 256x256");

    const Color bg = ref.getPixel(5, 5);
    require(bg.r == 255 && bg.g == 255 && bg.b == 255, "Background should be white");

    const Color face = ref.getPixel(128, 128);
    require(face.r == 255 && face.g == 220 && face.b == 40, "Face center should be yellow");

    const Color leftEye = ref.getPixel(92, 96);
    require(leftEye.r == 0 && leftEye.g == 0 && leftEye.b == 0, "Left eye should be black");

    const Color mouth = ref.getPixel(128, 188);
    require(mouth.r == 0 && mouth.g == 0 && mouth.b == 0, "Mouth sample should be black");
}

void testCodecRoundtripAgainstReference() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    PNGImage reference = api::createSmiley256PNG();

    BMPImage bmp = api::createSmiley256BMP();
    require(bmp.save(testOutDir + "/test_ref.bmp"), "Failed saving BMP in test");
    BMPImage bmpDecoded = BMPImage::load(testOutDir + "/test_ref.bmp");
    {
        const DiffStats s = compareImages(reference, bmpDecoded);
        require(s.maxAbs == 0, "BMP roundtrip must be pixel identical to reference");
    }

    PNGImage png = api::createSmiley256PNG();
    require(png.save(testOutDir + "/test_ref.png"), "Failed saving PNG in test");
    PNGImage pngDecoded = PNGImage::load(testOutDir + "/test_ref.png");
    {
        const DiffStats s = compareImages(reference, pngDecoded);
        require(s.maxAbs == 0, "PNG roundtrip must be pixel identical to reference");
    }

    GIFImage gif = api::createSmiley256GIF();
    require(gif.save(testOutDir + "/test_ref.gif"), "Failed saving GIF in test");
    GIFImage gifDecoded = GIFImage::load(testOutDir + "/test_ref.gif");
    {
        const DiffStats s = compareImages(reference, gifDecoded);
        require(s.maxAbs == 0, "GIF roundtrip must be pixel identical to reference");
    }

    JPGImage jpg = api::createSmiley256JPG();
    require(jpg.save(testOutDir + "/test_ref.jpg"), "Failed saving JPG in test");
    JPGImage jpgDecoded = JPGImage::load(testOutDir + "/test_ref.jpg");
    {
        const DiffStats s = compareImages(reference, jpgDecoded);
        std::cout << "JPEG diff stats mean=" << s.meanAbs << " max=" << s.maxAbs << "\n";
        require(s.meanAbs <= 12.0, "JPG mean absolute error is too high vs reference");
        require(s.maxAbs <= 180, "JPG max absolute channel error is too high vs reference");
    }
}

void testLayerBlendOutput() {
    PNGImage base = api::createSmiley256PNG();
    PNGImage blended = api::createLayerBlendDemoPNG();
    require(blended.width() == 256 && blended.height() == 256, "Layered blend image should be 256x256");

    const DiffStats s = compareImages(base, blended);
    require(s.meanAbs > 1.0, "Layered blend should visually differ from base smiley");
}

void testLayeredSmileyMatchesDirect() {
    PNGImage direct = api::createSmiley256PNG();
    PNGImage layered = api::createSmiley256LayeredPNG();
    const DiffStats s = compareImages(direct, layered);
    require(s.maxAbs == 0, "Layered smiley should match direct smiley pixel-for-pixel");
}
} // namespace

int main() {
    try {
        testReferenceSmileyShape();
        testCodecRoundtripAgainstReference();
        testLayerBlendOutput();
        testLayeredSmileyMatchesDirect();

        std::cout << "All tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << "\n";
        return 1;
    }
}
