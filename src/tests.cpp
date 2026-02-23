#include "example_api.h"
#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "layer.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
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

DiffStats compareImages(const Image& a, const Image& b) {
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
    PNGImage ref = example_api::createSmiley256PNG();

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
    PNGImage reference = example_api::createSmiley256PNG();

    BMPImage bmp = example_api::createSmiley256BMP();
    require(bmp.save(testOutDir + "/test_ref.bmp"), "Failed saving BMP in test");
    BMPImage bmpDecoded = BMPImage::load(testOutDir + "/test_ref.bmp");
    {
        const DiffStats s = compareImages(reference, bmpDecoded);
        require(s.maxAbs == 0, "BMP roundtrip must be pixel identical to reference");
    }

    PNGImage png = example_api::createSmiley256PNG();
    require(png.save(testOutDir + "/test_ref.png"), "Failed saving PNG in test");
    PNGImage pngDecoded = PNGImage::load(testOutDir + "/test_ref.png");
    {
        const DiffStats s = compareImages(reference, pngDecoded);
        require(s.maxAbs == 0, "PNG roundtrip must be pixel identical to reference");
    }

    GIFImage gif = example_api::createSmiley256GIF();
    require(gif.save(testOutDir + "/test_ref.gif"), "Failed saving GIF in test");
    GIFImage gifDecoded = GIFImage::load(testOutDir + "/test_ref.gif");
    {
        const DiffStats s = compareImages(reference, gifDecoded);
        require(s.maxAbs == 0, "GIF roundtrip must be pixel identical to reference");
    }

    JPGImage jpg = example_api::createSmiley256JPG();
    require(jpg.save(testOutDir + "/test_ref.jpg"), "Failed saving JPG in test");
    JPGImage jpgDecoded = JPGImage::load(testOutDir + "/test_ref.jpg");
    {
        const DiffStats s = compareImages(reference, jpgDecoded);
        std::cout << "JPEG diff stats mean=" << s.meanAbs << " max=" << s.maxAbs << "\n";
        require(s.meanAbs <= 12.0, "JPG mean absolute error is too high vs reference");
        require(s.maxAbs <= 180, "JPG max absolute channel error is too high vs reference");
    }

    if (WEBPImage::isToolingAvailable()) {
        WEBPImage webp = example_api::createSmiley256WEBP();
        require(webp.save(testOutDir + "/test_ref.webp"), "Failed saving WEBP in test");
        WEBPImage webpDecoded = WEBPImage::load(testOutDir + "/test_ref.webp");
        const DiffStats s = compareImages(reference, webpDecoded);
        require(s.maxAbs == 0, "WEBP lossless roundtrip must be pixel identical to reference");
    } else {
        std::cout << "Skipping WEBP roundtrip test (install cwebp and dwebp to enable)\n";
    }

    SVGImage svg = example_api::createSmiley256SVG();
    require(svg.save(testOutDir + "/test_ref.svg"), "Failed saving SVG in test");
    SVGImage svgDecoded = SVGImage::load(testOutDir + "/test_ref.svg");
    {
        const DiffStats s = compareImages(reference, svgDecoded);
        require(s.maxAbs == 0, "SVG roundtrip must be pixel identical to reference");
    }
}

void testSVGViewBoxFallback() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string svgPath = testOutDir + "/test_viewbox.svg";

    std::ofstream out(svgPath);
    require(static_cast<bool>(out), "Failed to open viewBox SVG for writing");
    out << "<svg viewBox=\"0 0 2 3\">"
           "<rect width=\"2\" height=\"3\" fill=\"rgb(10,20,30)\"/>"
           "</svg>";
    out.close();

    SVGImage image = SVGImage::load(svgPath);
    require(image.width() == 2 && image.height() == 3, "viewBox should provide SVG dimensions");
    const Color p = image.getPixel(0, 0);
    require(p.r == 10 && p.g == 20 && p.b == 30, "viewBox SVG should apply fill rect");
}

void testSVGTranslateTransform() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string svgPath = testOutDir + "/test_transform.svg";

    std::ofstream out(svgPath);
    require(static_cast<bool>(out), "Failed to open transform SVG for writing");
    out << "<svg width=\"4\" height=\"4\">"
           "<g transform=\"translate(1,2)\">"
           "<rect x=\"0\" y=\"0\" width=\"1\" height=\"1\" fill=\"rgb(5,10,15)\"/>"
           "</g>"
           "</svg>";
    out.close();

    SVGImage image = SVGImage::load(svgPath);
    require(image.width() == 4 && image.height() == 4, "Transform SVG dimensions should be 4x4");
    const Color p = image.getPixel(1, 2);
    require(p.r == 5 && p.g == 10 && p.b == 15, "translate() should offset rect position");
}

void testSVGRasterizeToRequestedSize() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string svgPath = testOutDir + "/test_rasterize_size.svg";

    std::ofstream out(svgPath);
    require(static_cast<bool>(out), "Failed to open rasterize SVG for writing");
    out << "<svg viewBox=\"0 0 10 10\" preserveAspectRatio=\"none\">"
           "<rect x=\"0\" y=\"0\" width=\"5\" height=\"10\" fill=\"rgb(255,0,0)\"/>"
           "<rect x=\"5\" y=\"0\" width=\"5\" height=\"10\" fill=\"rgb(0,0,255)\"/>"
           "</svg>";
    out.close();

    SVGImage rasterized = SVGImage::load(svgPath, 40, 20);
    require(rasterized.width() == 40 && rasterized.height() == 20, "Rasterized SVG should use requested dimensions");

    const Color left = rasterized.getPixel(5, 10);
    const Color right = rasterized.getPixel(35, 10);
    require(left.r == 255 && left.g == 0 && left.b == 0, "Left half should rasterize as red");
    require(right.r == 0 && right.g == 0 && right.b == 255, "Right half should rasterize as blue");
}

void testRasterizeSVGFileToRasterImage() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string svgPath = testOutDir + "/test_raster_to_raster.svg";

    std::ofstream out(svgPath);
    require(static_cast<bool>(out), "Failed to open raster-to-raster SVG for writing");
    out << "<svg viewBox=\"0 0 2 1\" preserveAspectRatio=\"none\">"
           "<rect x=\"0\" y=\"0\" width=\"1\" height=\"1\" fill=\"rgb(10,20,30)\"/>"
           "<rect x=\"1\" y=\"0\" width=\"1\" height=\"1\" fill=\"rgb(200,210,220)\"/>"
           "</svg>";
    out.close();

    PNGImage raster(20, 10, Color(0, 0, 0));
    rasterizeSVGFileToRaster(svgPath, raster);
    const Color left = raster.getPixel(2, 5);
    const Color right = raster.getPixel(18, 5);
    require(left.r == 10 && left.g == 20 && left.b == 30, "rasterizeSVGFileToRaster should write left region");
    require(right.r == 200 && right.g == 210 && right.b == 220, "rasterizeSVGFileToRaster should write right region");
}

void testRasterizeSVGFileToLayer() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string svgPath = testOutDir + "/test_raster_to_layer.svg";

    std::ofstream out(svgPath);
    require(static_cast<bool>(out), "Failed to open raster-to-layer SVG for writing");
    out << "<svg width=\"4\" height=\"4\">"
           "<rect x=\"1\" y=\"1\" width=\"2\" height=\"2\" fill=\"rgb(12,34,56)\"/>"
           "</svg>";
    out.close();

    Layer layer("Vector Layer", 4, 4, PixelRGBA8(0, 0, 0, 0));
    rasterizeSVGFileToLayer(svgPath, layer, 222);
    const PixelRGBA8 center = layer.image().getPixel(2, 2);
    require(center.r == 12 && center.g == 34 && center.b == 56, "rasterizeSVGFileToLayer should copy RGB");
    require(center.a == 222, "rasterizeSVGFileToLayer should set layer alpha");
}

void testLayerBlendOutput() {
    PNGImage base = example_api::createSmiley256PNG();
    PNGImage blended = example_api::createLayerBlendDemoPNG();
    require(blended.width() == 256 && blended.height() == 256, "Layered blend image should be 256x256");

    const DiffStats s = compareImages(base, blended);
    require(s.meanAbs > 1.0, "Layered blend should visually differ from base smiley");
}

void testLayeredSmileyMatchesDirect() {
    PNGImage direct = example_api::createSmiley256PNG();
    PNGImage layered = example_api::createSmiley256LayeredPNG();
    const DiffStats s = compareImages(direct, layered);
    require(s.maxAbs == 0, "Layered smiley should match direct smiley pixel-for-pixel");
}

void testLayerMaskVisibilityControl() {
    Document doc(2, 1);

    Layer background("Background", 2, 1, PixelRGBA8(10, 20, 30, 255));
    doc.addLayer(background);

    Layer fg("Foreground", 2, 1, PixelRGBA8(200, 100, 50, 255));
    fg.enableMask(PixelRGBA8(255, 255, 255, 255));
    fg.mask().setPixel(1, 0, PixelRGBA8(0, 0, 0, 255));
    doc.addLayer(fg);

    const ImageBuffer out = doc.composite();
    const PixelRGBA8 left = out.getPixel(0, 0);
    const PixelRGBA8 right = out.getPixel(1, 0);

    require(left.r == 200 && left.g == 100 && left.b == 50, "Layer mask should keep fully white mask pixels visible");
    require(right.r == 10 && right.g == 20 && right.b == 30, "Layer mask should hide fully black mask pixels");
}

void testLayerMaskCanBeCleared() {
    Document doc(1, 1);
    Layer base("Base", 1, 1, PixelRGBA8(0, 0, 0, 255));
    doc.addLayer(base);

    Layer fg("FG", 1, 1, PixelRGBA8(255, 0, 0, 255));
    fg.enableMask(PixelRGBA8(0, 0, 0, 255));
    fg.clearMask();
    doc.addLayer(fg);

    const ImageBuffer out = doc.composite();
    const PixelRGBA8 p = out.getPixel(0, 0);
    require(p.r == 255 && p.g == 0 && p.b == 0, "Clearing a mask should restore full layer visibility");
}

void testRasterResizeFilters() {
    PNGImage src(2, 2, Color(0, 0, 0));
    src.setPixel(0, 0, Color(0, 0, 0));
    src.setPixel(1, 0, Color(100, 0, 0));
    src.setPixel(0, 1, Color(0, 100, 0));
    src.setPixel(1, 1, Color(100, 100, 0));

    PNGImage nearest = resizeImage(src, 4, 4, ResizeFilter::Nearest);
    require(nearest.width() == 4 && nearest.height() == 4, "Nearest resize should produce requested dimensions");
    const Color n00 = nearest.getPixel(0, 0);
    const Color n11 = nearest.getPixel(1, 1);
    const Color n22 = nearest.getPixel(2, 2);
    require(n00.r == 0 && n00.g == 0 && n00.b == 0, "Nearest should map top-left to source 0,0");
    require(n11.r == 0 && n11.g == 0 && n11.b == 0, "Nearest should keep first 2x2 block from source 0,0");
    require(n22.r == 100 && n22.g == 100 && n22.b == 0, "Nearest should map bottom-right block to source 1,1");

    PNGImage bilinearDefault = resizeImage(src, 4, 4);
    const Color b11 = bilinearDefault.getPixel(1, 1);
    require(b11.r == 25 && b11.g == 25 && b11.b == 0, "Bilinear default should interpolate center of first quadrant");
    const Color b22 = bilinearDefault.getPixel(2, 2);
    require(b22.r == 75 && b22.g == 75 && b22.b == 0, "Bilinear should interpolate toward bottom-right");

    PNGImage boxAverage = resizeImage(src, 4, 4, ResizeFilter::BoxAverage);
    const Color a11 = boxAverage.getPixel(1, 1);
    const Color a22 = boxAverage.getPixel(2, 2);
    require(a11.r == 25 && a11.g == 25 && a11.b == 0, "BoxAverage should blend nearby pixels when upscaling");
    require(a22.r == 75 && a22.g == 75 && a22.b == 0, "BoxAverage should keep center region smooth");
}

void testLayerTransformRotation() {
    Document doc(5, 5);
    Layer layer("Dot", 5, 5, PixelRGBA8(0, 0, 0, 0));
    layer.image().setPixel(3, 2, PixelRGBA8(255, 0, 0, 255));
    layer.transform().setRotationDegrees(90.0, 2.0, 2.0);
    doc.addLayer(layer);

    const ImageBuffer out = doc.composite();
    const PixelRGBA8 p = out.getPixel(1, 3);
    require(p.r == 255 && p.g == 0 && p.b == 0, "Rotation transform should rotate pixels around pivot");
}

void testGroupTransformTranslate() {
    Document doc(4, 4);
    LayerGroup group("Group");
    group.transform().setTranslation(1.0, 1.0);

    Layer layer("Dot", 4, 4, PixelRGBA8(0, 0, 0, 0));
    layer.image().setPixel(0, 0, PixelRGBA8(0, 255, 0, 255));
    group.addLayer(layer);
    doc.addGroup(group);

    const ImageBuffer out = doc.composite();
    const PixelRGBA8 p = out.getPixel(1, 1);
    require(p.g == 255, "Group translation should offset child layers");
}

void testGroupedLayerOffsetAndVisibility() {
    Document doc(3, 1);
    doc.addLayer(Layer("Background", 3, 1, PixelRGBA8(5, 5, 5, 255)));

    LayerGroup group("Group A");
    group.setOffset(1, 0);
    group.setVisible(true);
    group.addLayer(Layer("Dot", 1, 1, PixelRGBA8(240, 0, 0, 255)));
    doc.addGroup(group);

    ImageBuffer out = doc.composite();
    require(out.getPixel(0, 0).r == 5, "Group offset should not affect untouched pixels");
    require(out.getPixel(1, 0).r == 240, "Grouped layer should render with group offset");

    doc.node(1).asGroup().setVisible(false);
    out = doc.composite();
    require(out.getPixel(1, 0).r == 5, "Hidden groups should not render");
}

void testGroupedLayerOpacityAffectsComposite() {
    Document doc(1, 1);
    doc.addLayer(Layer("Background", 1, 1, PixelRGBA8(0, 0, 0, 255)));

    LayerGroup group("Fade Group");
    group.setOpacity(0.5f);
    group.addLayer(Layer("White Pixel", 1, 1, PixelRGBA8(255, 255, 255, 255)));
    doc.addGroup(group);

    const PixelRGBA8 p = doc.composite().getPixel(0, 0);
    require(p.r == 188 && p.g == 188 && p.b == 188, "Group opacity should apply to the flattened group result");
}

void testIFLOWSerializationRoundtripPreservesStack() {
    const std::string testOutDir = "build/output/test-images";
    std::filesystem::create_directories(testOutDir);
    const std::string iflowPath = testOutDir + "/roundtrip.iflow";

    Document original(4, 2);
    original.addLayer(Layer("Background", 4, 2, PixelRGBA8(12, 24, 36, 255)));

    LayerGroup faceGroup("Face Group");
    faceGroup.setOffset(1, 0);
    faceGroup.setOpacity(0.8f);
    faceGroup.transform().setTranslation(0.0, 1.0);

    Layer fill("Fill", 2, 2, PixelRGBA8(220, 180, 80, 255));
    fill.transform().setTranslation(1.0, 0.0);
    fill.enableMask(PixelRGBA8(255, 255, 255, 255));
    fill.mask().setPixel(1, 1, PixelRGBA8(0, 0, 0, 255));
    faceGroup.addLayer(fill);

    Layer shade("Shade", 2, 2, PixelRGBA8(20, 40, 100, 180));
    shade.setBlendMode(BlendMode::Multiply);
    faceGroup.addLayer(shade);

    original.addGroup(faceGroup);

    require(saveDocumentIFLOW(original, iflowPath), "Saving IFLOW document should succeed");
    Document loaded = loadDocumentIFLOW(iflowPath);

    require(loaded.width() == original.width() && loaded.height() == original.height(), "IFLOW should preserve dimensions");
    require(loaded.nodeCount() == 2, "IFLOW should preserve root node count");
    require(loaded.node(1).isGroup(), "IFLOW should preserve group nodes");
    const LayerGroup& loadedGroup = loaded.node(1).asGroup();
    require(loadedGroup.nodeCount() == 2, "IFLOW should preserve group children");
    require(loadedGroup.node(0).asLayer().hasMask(), "IFLOW should preserve per-layer masks");

    const ImageBuffer a = original.composite();
    const ImageBuffer b = loaded.composite();
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            const PixelRGBA8 pa = a.getPixel(x, y);
            const PixelRGBA8 pb = b.getPixel(x, y);
            require(pa.r == pb.r && pa.g == pb.g && pa.b == pb.b && pa.a == pb.a, "IFLOW roundtrip should preserve composite output");
        }
    }
}
} // namespace

int main() {
    try {
        testReferenceSmileyShape();
        testCodecRoundtripAgainstReference();
        testSVGViewBoxFallback();
        testSVGTranslateTransform();
        testSVGRasterizeToRequestedSize();
        testRasterizeSVGFileToRasterImage();
        testRasterizeSVGFileToLayer();
        testLayerTransformRotation();
        testGroupTransformTranslate();
        testLayerBlendOutput();
        testLayeredSmileyMatchesDirect();
        testLayerMaskVisibilityControl();
        testLayerMaskCanBeCleared();
        testRasterResizeFilters();
        testGroupedLayerOffsetAndVisibility();
        testGroupedLayerOpacityAffectsComposite();
        testIFLOWSerializationRoundtripPreservesStack();

        std::cout << "All tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << "\n";
        return 1;
    }
}
