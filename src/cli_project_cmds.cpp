#include "cli_project_cmds.h"

#include "cli_args.h"
#include "cli_parse.h"
#include "cli_shared.h"
#include "layer.h"
#include "resize.h"

#include "bmp.h"
#include "gif.h"
#include "jpg.h"
#include "png.h"
#include "webp.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void resizeLayerForNew(Layer& layer, int width, int height, ResizeFilter filter) {
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
} // namespace

int runIFLOWNew(const std::vector<std::string>& args) {
    std::string widthValue;
    std::string heightValue;
    std::string outPath;
    std::string fromImagePath;
    std::string fitValue;
    const bool hasWidth = getFlagValue(args, "--width", widthValue);
    const bool hasHeight = getFlagValue(args, "--height", heightValue);
    const bool hasFromImage = getFlagValue(args, "--from-image", fromImagePath);
    const bool hasFit = getFlagValue(args, "--fit", fitValue);

    if (hasFromImage && (hasWidth || hasHeight)) {
        std::cerr << "Error: --from-image cannot be combined with --width/--height\n";
        return 1;
    }
    if (hasFit && !hasFromImage) {
        std::cerr << "Error: --fit requires --from-image\n";
        return 1;
    }

    if (!getFlagValue(args, "--out", outPath) || ((hasWidth != hasHeight) || (!hasFromImage && (!hasWidth || !hasHeight)))) {
        std::cerr << "Usage: image_flow new --width <w> --height <h> --out <project.iflow>\n"
                  << "   or: image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>\n";
        return 1;
    }

    int width = 0;
    int height = 0;
    Layer baseLayer;
    bool addBaseLayer = false;

    if (hasFromImage) {
        BMPImage bmp;
        PNGImage png;
        JPGImage jpg;
        GIFImage gif;
        WEBPImage webp;
        RasterImage* source = loadImageByExtension(fromImagePath, bmp, png, jpg, gif, webp);
        width = source->width();
        height = source->height();
        addBaseLayer = true;
        baseLayer = Layer("Base", width, height, PixelRGBA8(0, 0, 0, 0));
        baseLayer.setImageFromRaster(*source, 255);

        if (hasFit) {
            const std::size_t split = fitValue.find('x');
            if (split == std::string::npos || split == 0 || split + 1 >= fitValue.size()) {
                throw std::runtime_error("Invalid --fit value; expected <w>x<h>");
            }
            width = parseIntInRange(fitValue.substr(0, split), "fit width", 1, std::numeric_limits<int>::max());
            height = parseIntInRange(fitValue.substr(split + 1), "fit height", 1, std::numeric_limits<int>::max());
            const ResizeFilter filter = ResizeFilter::Bilinear;
            resizeLayerForNew(baseLayer, width, height, filter);
        }
    } else {
        width = parseIntInRange(widthValue, "width", 1, std::numeric_limits<int>::max());
        height = parseIntInRange(heightValue, "height", 1, std::numeric_limits<int>::max());
    }

    Document document(width, height);
    if (addBaseLayer) {
        document.addLayer(baseLayer);
    }
    if (!saveDocumentIFLOW(document, outPath)) {
        std::cerr << "Failed saving IFLOW document: " << outPath << "\n";
        return 1;
    }

    std::cout << "Created IFLOW project " << outPath << " (" << width << "x" << height << ")";
    if (addBaseLayer) {
        std::cout << " with imported base layer";
    }
    std::cout << "\n";
    return 0;
}

int runIFLOWInfo(const std::vector<std::string>& args) {
    std::string inPath;
    if (!getFlagValue(args, "--in", inPath)) {
        std::cerr << "Usage: image_flow info --in <project.iflow>\n";
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
        std::cerr << "Usage: image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n";
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
