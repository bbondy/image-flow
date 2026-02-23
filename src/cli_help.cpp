#include "cli_help.h"

#include <iostream>

void writeUsage() {
    std::cout
        << "image_flow CLI\n\n"
        << "Usage:\n"
        << "  image_flow help\n"
        << "  image_flow help ops\n"
        << "  image_flow new --width <w> --height <h> --out <project.iflow>\n"
        << "  image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>\n"
        << "  image_flow info --in <project.iflow>\n"
        << "  image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>\n"
        << "  image_flow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n\n"
        << "  image_flow ops --width <w> --height <h> --out <project.iflow> [--op ...|--ops-file <path>|--stdin]\n\n"
        << "Notes:\n"
        << "  - WebP output requires cwebp/dwebp tooling in PATH.\n";
}

void writeOpsUsage() {
    std::cout
        << "image_flow ops reference\n\n"
        << "Usage:\n"
        << "  image_flow ops --in <project.iflow> --out <project.iflow> --op \"<action key=value ...>\" [--op ...]\n"
        << "  image_flow ops --width <w> --height <h> --out <project.iflow> [--op ...|--ops-file <path>|--stdin]\n\n"
        << "Input modes:\n"
        << "  - Use --in for existing projects.\n"
        << "  - Use --width/--height to start a new in-memory document.\n"
        << "  - --in and --width/--height are mutually exclusive.\n\n"
        << "Op sources:\n"
        << "  - --op \"...\" (repeatable)\n"
        << "  - --ops-file <path> (one op per line, '#' comments supported)\n"
        << "  - --stdin (one op per line)\n\n"
        << "Tokenization rules:\n"
        << "  - Op tokens are key=value pairs separated by spaces.\n"
        << "  - Quote values containing spaces: name=\"Layer One\" or name='Layer One'.\n"
        << "  - Escape quote/backslash inside values with backslash.\n\n"
        << "Common actions:\n"
        << "  - Structure: add-layer add-group add-grid-layers set-layer set-group import-image resize-layer\n"
        << "  - Transform: set-transform concat-transform clear-transform\n"
        << "  - Drawing: draw-fill draw-line draw-rect draw-fill-rect draw-round-rect draw-fill-round-rect draw-ellipse\n"
        << "             draw-fill-ellipse draw-polyline draw-polygon draw-fill-polygon draw-flood-fill draw-circle\n"
        << "             draw-fill-circle draw-arc draw-quadratic-bezier draw-bezier\n"
        << "  - Effects: apply-effect replace-color channel-mix levels gamma curves gaussian-blur edge-detect morphology\n"
        << "             fractal-noise hatch pencil-strokes noise-layer checker-layer gradient-layer\n"
        << "  - Pixel/mask: fill-layer set-pixel mask-enable mask-clear mask-set-pixel\n"
        << "  - Output: emit\n\n"
        << "Example:\n"
        << "  image_flow ops --in in.iflow --out out.iflow \\\n"
        << "    --op \"add-layer parent=/ name=Sketch width=800 height=600 fill=0,0,0,0\" \\\n"
        << "    --op \"draw-fill-rect path=/0 x=40 y=30 width=220 height=140 rgba=255,180,20,220\"\n";
}
