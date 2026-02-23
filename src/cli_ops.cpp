#include "cli.h"
#include "cli_args.h"
#include "cli_help.h"
#include "cli_parse.h"
#include "cli_ops_core.h"
#include "cli_project_cmds.h"
#include "cli_shared.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
int runIFLOWOpsImpl(const std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "--help") != args.end() ||
        std::find(args.begin(), args.end(), "-h") != args.end()) {
        writeOpsUsage();
        return 0;
    }

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
    const std::vector<std::string> opSpecs = gatherOps(args);
    if (hasIn && (hasWidth || hasHeight)) {
        std::cerr << "Error: --in cannot be combined with --width/--height for ops\n";
        return 1;
    }

    if (!hasOut || opSpecs.empty() || (!hasIn && (!hasWidth || !hasHeight))) {
        std::cerr << "Usage: image_flow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n"
                  << "   or: image_flow ops --width <w> --height <h> --out <project.iflow> [--op ...|--ops-file <path>|--stdin]\n";
        return 1;
    }

    Document document = hasIn
                            ? loadDocumentIFLOW(inPath)
                            : Document(parseIntInRange(widthValue, "width", 1, std::numeric_limits<int>::max()),
                                       parseIntInRange(heightValue, "height", 1, std::numeric_limits<int>::max()));
    std::size_t emitCount = 0;
    const auto emitOutput = [&](const std::string& outputPath) {
        const ImageBuffer composite = document.composite();
        const std::filesystem::path outFsPath(outputPath);
        if (outFsPath.has_parent_path()) {
            std::filesystem::create_directories(outFsPath.parent_path());
        }
        if (!saveCompositeByExtension(composite, outputPath)) {
            throw std::runtime_error("Failed writing emit output: " + outputPath);
        }
        ++emitCount;
        std::cout << "Emitted " << outputPath << "\n";
    };
    for (std::size_t i = 0; i < opSpecs.size(); ++i) {
        try {
            applyDocumentOperation(document, opSpecs[i], emitOutput);
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

    std::cout << "Saved " << outPath << " after " << opSpecs.size() << " ops";
    if (emitCount > 0) {
        std::cout << " and " << emitCount << " emit outputs";
    }
    std::cout << "\n";
    return 0;
}
} // namespace

int runIFLOWOps(const std::vector<std::string>& args) {
    return runIFLOWOpsImpl(args);
}
