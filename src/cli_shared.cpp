#include "cli_shared.h"

#include "svg.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
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

const char* blendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal:
            return "normal";
        case BlendMode::Multiply:
            return "multiply";
        case BlendMode::Screen:
            return "screen";
        case BlendMode::Overlay:
            return "overlay";
        case BlendMode::Darken:
            return "darken";
        case BlendMode::Lighten:
            return "lighten";
        case BlendMode::Add:
            return "add";
        case BlendMode::Subtract:
            return "subtract";
        case BlendMode::Difference:
            return "difference";
        case BlendMode::ColorDodge:
            return "color-dodge";
        default:
            return "unknown";
    }
}
} // namespace

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

RasterImage* loadImageByExtension(const std::string& imagePath, BMPImage& bmp, PNGImage& png, JPGImage& jpg, GIFImage& gif, WEBPImage& webp) {
    const std::string ext = extensionLower(imagePath);
    if (ext == "bmp") {
        bmp = BMPImage::load(imagePath);
        return &bmp;
    }
    if (ext == "png") {
        png = PNGImage::load(imagePath);
        return &png;
    }
    if (ext == "jpg" || ext == "jpeg") {
        jpg = JPGImage::load(imagePath);
        return &jpg;
    }
    if (ext == "gif") {
        gif = GIFImage::load(imagePath);
        return &gif;
    }
    if (ext == "webp") {
        if (!WEBPImage::isToolingAvailable()) {
            throw std::runtime_error("WebP tooling unavailable (install cwebp and dwebp)");
        }
        webp = WEBPImage::load(imagePath);
        return &webp;
    }
    throw std::runtime_error("Unsupported image format for --from-image: " + imagePath);
}

void printGroupInfo(const LayerGroup& group, const std::string& indent) {
    std::cout << indent << "Group '" << group.name() << "'"
              << " nodes=" << group.nodeCount()
              << " visible=" << (group.visible() ? "true" : "false")
              << " opacity=" << group.opacity()
              << " blendMode=" << blendModeName(group.blendMode())
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
                  << " blendMode=" << blendModeName(layer.blendMode())
                  << " offset=(" << layer.offsetX() << "," << layer.offsetY() << ")"
                  << " mask=" << (layer.hasMask() ? "true" : "false") << "\n";
    }
}
