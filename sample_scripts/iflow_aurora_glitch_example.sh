#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_aurora_glitch.iflow"
RENDER_PATH="$OUT_DIR/tahoe_aurora_glitch.png"
OPS_PATH="$OUT_DIR/tahoe_aurora_glitch.ops"

WIDTH=1600
HEIGHT=900

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "Missing input image: $INPUT_IMAGE" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR"

"$BIN" new --from-image "$INPUT_IMAGE" --fit "${WIDTH}x${HEIGHT}" --out "$PROJECT_PATH"

cat > "$OPS_PATH" <<OPS
# Layer 0 is the imported Tahoe image.

# Grade the base with subtle monochrome noise so everything feels analog.
noise-layer path=/0 seed=9123 amount=0.18 monochrome=true

# Aurora light wash.
add-layer name=Aurora width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/1 type=radial from=10,255,230,170 to=110,20,190,0 center=420,260 radius=980
set-layer path=/1 blend=screen opacity=0.74
set-transform path=/1 rotate=-8 pivot=800,450

# Thin scanline texture.
add-layer name=Scanlines width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
checker-layer path=/2 cell_width=${WIDTH} cell_height=3 a=0,0,0,0 b=30,255,170,34
set-layer path=/2 blend=overlay opacity=0.66

# Offset ghost pass from the same input image.
add-layer name=Ghost width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
set-layer path=/3 blend=add opacity=0.24
set-transform path=/3 translate=20,-14

# Vignette to pull focus.
add-layer name=Vignette width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/4 type=radial from=0,0,0,0 to=0,0,0,215 center=800,450 radius=740
set-layer path=/4 blend=multiply opacity=0.90
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
