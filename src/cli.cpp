#include "cli.h"

#include "bmp.h"
#include "effects.h"
#include "example_api.h"
#include "gif.h"
#include "jpg.h"
#include "layer.h"
#include "png.h"
#include "resize.h"
#include "svg.h"
#include "webp.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string extensionLower(const std::string& path) {
    const std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext.erase(0, 1);
    }
    return toLower(ext);
}

std::vector<std::string> collectArgs(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

bool getFlagValue(const std::vector<std::string>& args, const std::string& flag, std::string& value) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            value = args[i + 1];
            return true;
        }
    }
    return false;
}

std::vector<std::string> getFlagValues(const std::vector<std::string>& args, const std::string& flag) {
    std::vector<std::string> values;
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            values.push_back(args[i + 1]);
        }
    }
    return values;
}

void writeUsage() {
    std::cout
        << "image-flow CLI\n\n"
        << "Usage:\n"
        << "  image-flow help\n"
        << "  image-flow samples\n"
        << "  image-flow iflow new --width <w> --height <h> --out <project.iflow>\n"
        << "  image-flow iflow info --in <project.iflow>\n"
        << "  image-flow iflow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n\n"
        << "  image-flow iflow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n\n"
        << "Notes:\n"
        << "  - Running with no subcommand defaults to 'samples'.\n"
        << "  - WebP output requires cwebp/dwebp tooling in PATH.\n";
}

void copyBufferToImage(const ImageBuffer& source, Image& destination) {
    if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::invalid_argument("copyBufferToImage dimensions must match");
    }
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = source.getPixel(x, y);
            destination.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
}

bool saveCompositeByExtension(const ImageBuffer& composite, const std::string& outPath) {
    const std::string ext = extensionLower(outPath);

    if (ext == "png") {
        PNGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "bmp") {
        BMPImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "jpg" || ext == "jpeg") {
        JPGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "gif") {
        GIFImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        WEBPImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyToRasterImage(composite, out);
        return out.save(outPath);
    }
    if (ext == "svg") {
        SVGImage out(composite.width(), composite.height(), Color(0, 0, 0));
        copyBufferToImage(composite, out);
        return out.save(outPath);
    }

    throw std::runtime_error("Unsupported output extension: " + ext);
}

void printGroupInfo(const LayerGroup& group, const std::string& indent) {
    std::cout << indent << "Group '" << group.name() << "'"
              << " nodes=" << group.nodeCount()
              << " visible=" << (group.visible() ? "true" : "false")
              << " opacity=" << group.opacity()
              << " blendMode=" << static_cast<int>(group.blendMode())
              << " offset=(" << group.offsetX() << "," << group.offsetY() << ")\n";

    for (std::size_t i = 0; i < group.nodeCount(); ++i) {
        const LayerNode& node = group.node(i);
        if (node.isGroup()) {
            printGroupInfo(node.asGroup(), indent + "  ");
            continue;
        }
        const Layer& layer = node.asLayer();
        std::cout << indent << "  Layer '" << layer.name() << "'"
                  << " size=" << layer.image().width() << "x" << layer.image().height()
                  << " visible=" << (layer.visible() ? "true" : "false")
                  << " opacity=" << layer.opacity()
                  << " blendMode=" << static_cast<int>(layer.blendMode())
                  << " offset=(" << layer.offsetX() << "," << layer.offsetY() << ")"
                  << " mask=" << (layer.hasMask() ? "true" : "false") << "\n";
    }
}

PNGImage rasterToPNG(const RasterImage& src) {
    PNGImage out(src.width(), src.height(), Color(0, 0, 0));
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            out.setPixel(x, y, src.getPixel(x, y));
        }
    }
    return out;
}

int runSamplesCommand() {
    const std::string outDir = "build/output/images";
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
                 "build/output/images/tahoe200-original.png, build/output/images/tahoe200-grayscale.png, "
                 "build/output/images/tahoe200-sepia.png, and build/output/images/tahoe200-sepia-layered.png ("
              << bmpDecoded.width() << "x" << bmpDecoded.height() << ")\n";
    std::cout << "Layered vs direct smiley diff: mean=" << meanDiff << " max=" << maxDiff << "\n";

    return 0;
}

std::vector<std::string> splitWhitespace(const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> parts;
    std::string token;
    while (in >> token) {
        parts.push_back(token);
    }
    return parts;
}

std::vector<std::string> splitByChar(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream in(text);
    while (std::getline(in, current, delimiter)) {
        parts.push_back(current);
    }
    return parts;
}

bool parseBoolFlag(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    throw std::runtime_error("Invalid boolean value: " + value);
}

std::pair<int, int> parseIntPair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected integer pair x,y but got: " + text);
    }
    return {std::stoi(parts[0]), std::stoi(parts[1])};
}

std::pair<double, double> parseDoublePair(const std::string& text) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Expected numeric pair x,y but got: " + text);
    }
    return {std::stod(parts[0]), std::stod(parts[1])};
}

PixelRGBA8 parseRGBA(const std::string& text, bool allowRgb = false) {
    const std::vector<std::string> parts = splitByChar(text, ',');
    if (parts.size() == 3 && allowRgb) {
        return PixelRGBA8(static_cast<std::uint8_t>(std::stoi(parts[0])),
                          static_cast<std::uint8_t>(std::stoi(parts[1])),
                          static_cast<std::uint8_t>(std::stoi(parts[2])),
                          255);
    }
    if (parts.size() != 4) {
        throw std::runtime_error("Expected rgba=r,g,b,a but got: " + text);
    }
    return PixelRGBA8(static_cast<std::uint8_t>(std::stoi(parts[0])),
                      static_cast<std::uint8_t>(std::stoi(parts[1])),
                      static_cast<std::uint8_t>(std::stoi(parts[2])),
                      static_cast<std::uint8_t>(std::stoi(parts[3])));
}

std::vector<std::size_t> parsePathIndices(const std::string& path) {
    if (path.empty() || path[0] != '/') {
        throw std::runtime_error("Path must start with '/': " + path);
    }
    if (path == "/") {
        return {};
    }

    std::vector<std::size_t> indices;
    std::size_t start = 1;
    while (start < path.size()) {
        std::size_t end = path.find('/', start);
        const std::string piece = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (piece.empty()) {
            throw std::runtime_error("Invalid empty segment in path: " + path);
        }
        const int value = std::stoi(piece);
        if (value < 0) {
            throw std::runtime_error("Negative path segment in path: " + path);
        }
        indices.push_back(static_cast<std::size_t>(value));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return indices;
}

LayerGroup& resolveGroupPath(Document& document, const std::string& path) {
    LayerGroup* group = &document.rootGroup();
    const std::vector<std::size_t> indices = parsePathIndices(path);
    for (std::size_t index : indices) {
        LayerNode& node = group->node(index);
        if (!node.isGroup()) {
            throw std::runtime_error("Path does not resolve to group: " + path);
        }
        group = &node.asGroup();
    }
    return *group;
}

LayerNode& resolveNodePath(Document& document, const std::string& path) {
    const std::vector<std::size_t> indices = parsePathIndices(path);
    if (indices.empty()) {
        throw std::runtime_error("Path '/' resolves to root group, not a node");
    }

    LayerGroup* group = &document.rootGroup();
    for (std::size_t i = 0; i + 1 < indices.size(); ++i) {
        LayerNode& node = group->node(indices[i]);
        if (!node.isGroup()) {
            throw std::runtime_error("Intermediate path segment must be a group: " + path);
        }
        group = &node.asGroup();
    }
    return group->node(indices.back());
}

Layer& resolveLayerPath(Document& document, const std::string& path) {
    LayerNode& node = resolveNodePath(document, path);
    if (!node.isLayer()) {
        throw std::runtime_error("Path does not resolve to layer: " + path);
    }
    return node.asLayer();
}

BlendMode parseBlendMode(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "normal") return BlendMode::Normal;
    if (lowered == "multiply") return BlendMode::Multiply;
    if (lowered == "screen") return BlendMode::Screen;
    if (lowered == "overlay") return BlendMode::Overlay;
    if (lowered == "darken") return BlendMode::Darken;
    if (lowered == "lighten") return BlendMode::Lighten;
    if (lowered == "add") return BlendMode::Add;
    if (lowered == "subtract") return BlendMode::Subtract;
    if (lowered == "difference") return BlendMode::Difference;
    throw std::runtime_error("Unsupported blend mode: " + value);
}

std::unordered_map<std::string, std::string> parseKeyValues(const std::vector<std::string>& tokens, std::size_t startIndex) {
    std::unordered_map<std::string, std::string> kv;
    for (std::size_t i = startIndex; i < tokens.size(); ++i) {
        const std::size_t split = tokens[i].find('=');
        if (split == std::string::npos || split == 0 || split + 1 >= tokens[i].size()) {
            throw std::runtime_error("Expected key=value token but got: " + tokens[i]);
        }
        kv[tokens[i].substr(0, split)] = tokens[i].substr(split + 1);
    }
    return kv;
}

ResizeFilter parseResizeFilter(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "nearest") return ResizeFilter::Nearest;
    if (lowered == "bilinear") return ResizeFilter::Bilinear;
    if (lowered == "box" || lowered == "boxaverage" || lowered == "box_average") return ResizeFilter::BoxAverage;
    throw std::runtime_error("Unsupported resize filter: " + value);
}

void importImageIntoLayer(Layer& layer, const std::string& imagePath, std::uint8_t alpha, const std::unordered_map<std::string, std::string>& kv) {
    const std::string ext = extensionLower(imagePath);
    if (ext == "png") {
        layer.setImageFromRaster(PNGImage::load(imagePath), alpha);
        return;
    }
    if (ext == "bmp") {
        layer.setImageFromRaster(BMPImage::load(imagePath), alpha);
        return;
    }
    if (ext == "jpg" || ext == "jpeg") {
        layer.setImageFromRaster(JPGImage::load(imagePath), alpha);
        return;
    }
    if (ext == "gif") {
        layer.setImageFromRaster(GIFImage::load(imagePath), alpha);
        return;
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        layer.setImageFromRaster(WEBPImage::load(imagePath), alpha);
        return;
    }
    if (ext == "svg") {
        int rasterWidth = layer.image().width();
        int rasterHeight = layer.image().height();
        const auto widthIt = kv.find("width");
        const auto heightIt = kv.find("height");
        if (widthIt != kv.end()) {
            rasterWidth = std::stoi(widthIt->second);
        }
        if (heightIt != kv.end()) {
            rasterHeight = std::stoi(heightIt->second);
        }
        SVGImage svg = SVGImage::load(imagePath, rasterWidth, rasterHeight);
        ImageBuffer buffer(svg.width(), svg.height(), PixelRGBA8(0, 0, 0, alpha));
        for (int y = 0; y < svg.height(); ++y) {
            for (int x = 0; x < svg.width(); ++x) {
                const Color& c = svg.getPixel(x, y);
                buffer.setPixel(x, y, PixelRGBA8(c.r, c.g, c.b, alpha));
            }
        }
        layer.image() = buffer;
        if (layer.hasMask()) {
            layer.clearMask();
        }
        return;
    }
    throw std::runtime_error("Unsupported import extension: " + ext);
}

void resizeLayer(Layer& layer, int width, int height, ResizeFilter filter) {
    PNGImage source(layer.image().width(), layer.image().height(), Color(0, 0, 0));
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const PixelRGBA8& p = layer.image().getPixel(x, y);
            source.setPixel(x, y, Color(p.r, p.g, p.b));
        }
    }
    PNGImage resized = resizeImage(source, width, height, filter);
    layer.setImageFromRaster(resized, 255);
}

void applyOperation(Document& document, const std::string& opSpec) {
    const std::vector<std::string> tokens = splitWhitespace(opSpec);
    if (tokens.empty()) {
        throw std::runtime_error("Empty --op value");
    }

    const std::string action = tokens[0];
    const std::unordered_map<std::string, std::string> kv = parseKeyValues(tokens, 1);

    if (action == "add-layer") {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Layer" : kv.at("name");
        const int width = kv.find("width") == kv.end() ? document.width() : std::stoi(kv.at("width"));
        const int height = kv.find("height") == kv.end() ? document.height() : std::stoi(kv.at("height"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(0, 0, 0, 0) : parseRGBA(kv.at("fill"));
        resolveGroupPath(document, parentPath).addLayer(Layer(name, width, height, fill));
        return;
    }

    if (action == "add-group") {
        const auto parentIt = kv.find("parent");
        const std::string parentPath = parentIt == kv.end() ? "/" : parentIt->second;
        const std::string name = kv.find("name") == kv.end() ? "Group" : kv.at("name");
        resolveGroupPath(document, parentPath).addGroup(LayerGroup(name));
        return;
    }

    if (action == "set-layer") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-layer requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        if (kv.find("name") != kv.end()) layer.setName(kv.at("name"));
        if (kv.find("visible") != kv.end()) layer.setVisible(parseBoolFlag(kv.at("visible")));
        if (kv.find("opacity") != kv.end()) layer.setOpacity(std::stof(kv.at("opacity")));
        if (kv.find("blend") != kv.end()) layer.setBlendMode(parseBlendMode(kv.at("blend")));
        if (kv.find("offset") != kv.end()) {
            const std::pair<int, int> offset = parseIntPair(kv.at("offset"));
            layer.setOffset(offset.first, offset.second);
        }
        return;
    }

    if (action == "set-group") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-group requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        if (!node.isGroup()) {
            throw std::runtime_error("set-group path must resolve to a group");
        }
        LayerGroup& group = node.asGroup();
        if (kv.find("name") != kv.end()) group.setName(kv.at("name"));
        if (kv.find("visible") != kv.end()) group.setVisible(parseBoolFlag(kv.at("visible")));
        if (kv.find("opacity") != kv.end()) group.setOpacity(std::stof(kv.at("opacity")));
        if (kv.find("blend") != kv.end()) group.setBlendMode(parseBlendMode(kv.at("blend")));
        if (kv.find("offset") != kv.end()) {
            const std::pair<int, int> offset = parseIntPair(kv.at("offset"));
            group.setOffset(offset.first, offset.second);
        }
        return;
    }

    if (action == "set-transform") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("set-transform requires path=");
        }
        LayerNode& node = resolveNodePath(document, kv.at("path"));
        Transform2D transform;
        if (kv.find("matrix") != kv.end()) {
            const std::vector<std::string> parts = splitByChar(kv.at("matrix"), ',');
            if (parts.size() != 6) {
                throw std::runtime_error("matrix= expects 6 comma-separated values");
            }
            transform = Transform2D::fromMatrix(std::stod(parts[0]), std::stod(parts[1]), std::stod(parts[2]),
                                                std::stod(parts[3]), std::stod(parts[4]), std::stod(parts[5]));
        } else {
            transform.setIdentity();
            if (kv.find("translate") != kv.end()) {
                const std::pair<double, double> t = parseDoublePair(kv.at("translate"));
                transform.translate(t.first, t.second);
            }
            if (kv.find("rotate") != kv.end()) {
                std::pair<double, double> pivot(0.0, 0.0);
                if (kv.find("pivot") != kv.end()) {
                    pivot = parseDoublePair(kv.at("pivot"));
                }
                transform.rotateDegrees(std::stod(kv.at("rotate")), pivot.first, pivot.second);
            }
        }
        if (node.isLayer()) {
            node.asLayer().transform() = transform;
        } else {
            node.asGroup().transform() = transform;
        }
        return;
    }

    if (action == "apply-effect") {
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
        throw std::runtime_error("Unsupported effect: " + effect);
    }

    if (action == "fill-layer") {
        if (kv.find("path") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("fill-layer requires path= and rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().fill(parseRGBA(kv.at("rgba")));
        return;
    }

    if (action == "set-pixel") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        layer.image().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    if (action == "mask-enable") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-enable requires path=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const PixelRGBA8 fill = kv.find("fill") == kv.end() ? PixelRGBA8(255, 255, 255, 255) : parseRGBA(kv.at("fill"));
        layer.enableMask(fill);
        return;
    }

    if (action == "mask-clear") {
        if (kv.find("path") == kv.end()) {
            throw std::runtime_error("mask-clear requires path=");
        }
        resolveLayerPath(document, kv.at("path")).clearMask();
        return;
    }

    if (action == "mask-set-pixel") {
        if (kv.find("path") == kv.end() || kv.find("x") == kv.end() || kv.find("y") == kv.end() || kv.find("rgba") == kv.end()) {
            throw std::runtime_error("mask-set-pixel requires path= x= y= rgba=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        if (!layer.hasMask()) {
            layer.enableMask();
        }
        layer.mask().setPixel(std::stoi(kv.at("x")), std::stoi(kv.at("y")), parseRGBA(kv.at("rgba")));
        return;
    }

    if (action == "import-image") {
        if (kv.find("path") == kv.end() || kv.find("file") == kv.end()) {
            throw std::runtime_error("import-image requires path= and file=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const std::uint8_t alpha = kv.find("alpha") == kv.end() ? 255 : static_cast<std::uint8_t>(std::stoi(kv.at("alpha")));
        importImageIntoLayer(layer, kv.at("file"), alpha, kv);
        return;
    }

    if (action == "resize-layer") {
        if (kv.find("path") == kv.end() || kv.find("width") == kv.end() || kv.find("height") == kv.end()) {
            throw std::runtime_error("resize-layer requires path= width= height=");
        }
        Layer& layer = resolveLayerPath(document, kv.at("path"));
        const ResizeFilter filter = kv.find("filter") == kv.end() ? ResizeFilter::Bilinear : parseResizeFilter(kv.at("filter"));
        resizeLayer(layer, std::stoi(kv.at("width")), std::stoi(kv.at("height")), filter);
        return;
    }

    throw std::runtime_error("Unknown op action: " + action);
}

int runIFLOWOps(const std::vector<std::string>& args) {
    std::string inPath;
    std::string outPath;
    std::string widthValue;
    std::string heightValue;
    std::string renderPath;
    const bool hasIn = getFlagValue(args, "--in", inPath);
    const bool hasOut = getFlagValue(args, "--out", outPath);
    const bool hasWidth = getFlagValue(args, "--width", widthValue);
    const bool hasHeight = getFlagValue(args, "--height", heightValue);
    const bool hasRender = getFlagValue(args, "--render", renderPath);
    const std::vector<std::string> opSpecs = getFlagValues(args, "--op");

    if (!hasOut || opSpecs.empty() || (!hasIn && (!hasWidth || !hasHeight))) {
        std::cerr << "Usage: image-flow iflow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n"
                  << "   or: image-flow iflow ops --width <w> --height <h> --out <project.iflow> --op \"...\"\n";
        return 1;
    }

    Document document = hasIn ? loadDocumentIFLOW(inPath) : Document(std::stoi(widthValue), std::stoi(heightValue));
    for (std::size_t i = 0; i < opSpecs.size(); ++i) {
        try {
            applyOperation(document, opSpecs[i]);
        } catch (const std::exception& ex) {
            std::ostringstream error;
            error << "Failed op[" << i << "] \"" << opSpecs[i] << "\": " << ex.what();
            throw std::runtime_error(error.str());
        }
    }

    const std::filesystem::path outFsPath(outPath);
    if (outFsPath.has_parent_path()) {
        std::filesystem::create_directories(outFsPath.parent_path());
    }
    if (!saveDocumentIFLOW(document, outPath)) {
        std::cerr << "Failed saving IFLOW document: " << outPath << "\n";
        return 1;
    }

    if (hasRender) {
        const ImageBuffer composite = document.composite();
        const std::filesystem::path renderFsPath(renderPath);
        if (renderFsPath.has_parent_path()) {
            std::filesystem::create_directories(renderFsPath.parent_path());
        }
        if (!saveCompositeByExtension(composite, renderPath)) {
            std::cerr << "Failed writing render output: " << renderPath << "\n";
            return 1;
        }
        std::cout << "Saved " << outPath << " and rendered " << renderPath << "\n";
        return 0;
    }

    std::cout << "Saved " << outPath << " after " << opSpecs.size() << " ops\n";
    return 0;
}

int runIFLOWNew(const std::vector<std::string>& args) {
    std::string widthValue;
    std::string heightValue;
    std::string outPath;
    if (!getFlagValue(args, "--width", widthValue) ||
        !getFlagValue(args, "--height", heightValue) ||
        !getFlagValue(args, "--out", outPath)) {
        std::cerr << "Usage: image-flow iflow new --width <w> --height <h> --out <project.iflow>\n";
        return 1;
    }

    const int width = std::stoi(widthValue);
    const int height = std::stoi(heightValue);
    Document document(width, height);
    if (!saveDocumentIFLOW(document, outPath)) {
        std::cerr << "Failed saving IFLOW document: " << outPath << "\n";
        return 1;
    }

    std::cout << "Created IFLOW project " << outPath << " (" << width << "x" << height << ")\n";
    return 0;
}

int runIFLOWInfo(const std::vector<std::string>& args) {
    std::string inPath;
    if (!getFlagValue(args, "--in", inPath)) {
        std::cerr << "Usage: image-flow iflow info --in <project.iflow>\n";
        return 1;
    }

    Document document = loadDocumentIFLOW(inPath);
    std::cout << "Document: " << inPath << "\n";
    std::cout << "Size: " << document.width() << "x" << document.height() << "\n";
    printGroupInfo(document.rootGroup(), "");
    return 0;
}

int runIFLOWRender(const std::vector<std::string>& args) {
    std::string inPath;
    std::string outPath;
    if (!getFlagValue(args, "--in", inPath) || !getFlagValue(args, "--out", outPath)) {
        std::cerr << "Usage: image-flow iflow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n";
        return 1;
    }

    Document document = loadDocumentIFLOW(inPath);
    ImageBuffer composite = document.composite();

    const std::filesystem::path outFsPath(outPath);
    if (outFsPath.has_parent_path()) {
        std::filesystem::create_directories(outFsPath.parent_path());
    }

    if (!saveCompositeByExtension(composite, outPath)) {
        std::cerr << "Failed writing image output: " << outPath << "\n";
        return 1;
    }

    std::cout << "Rendered " << inPath << " -> " << outPath << "\n";
    return 0;
}

int runIFLOWCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: image-flow iflow <new|info|render|ops> ...\n";
        return 1;
    }

    const std::string sub = args[1];
    if (sub == "new") {
        return runIFLOWNew(args);
    }
    if (sub == "info") {
        return runIFLOWInfo(args);
    }
    if (sub == "render") {
        return runIFLOWRender(args);
    }
    if (sub == "ops") {
        return runIFLOWOps(args);
    }

    std::cerr << "Unknown iflow subcommand: " << sub << "\n";
    return 1;
}
} // namespace

int runCLI(int argc, char** argv) {
    try {
        const std::vector<std::string> args = collectArgs(argc, argv);
        if (args.size() <= 1) {
            return runSamplesCommand();
        }

        const std::string command = args[1];
        if (command == "help" || command == "--help" || command == "-h") {
            writeUsage();
            return 0;
        }
        if (command == "samples") {
            return runSamplesCommand();
        }
        if (command == "iflow") {
            return runIFLOWCommand(std::vector<std::string>(args.begin() + 1, args.end()));
        }

        std::cerr << "Unknown command: " << command << "\n\n";
        writeUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
