#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_kaleido_gridstorm.iflow"
RENDER_PATH="$OUT_DIR/tahoe_kaleido_gridstorm.png"
OPS_PATH="$OUT_DIR/tahoe_kaleido_gridstorm.ops"

WIDTH=1200
HEIGHT=1200

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

# Build a high-density prism field (grid is only one ingredient).
add-group name=PrismField
add-grid-layers parent=/1 rows=24 cols=24 tile_width=50 tile_height=50 border=11 name_prefix=Prism opacity=0.55 fills=255,70,120,165;80,235,255,150;250,245,90,155;120,255,175,150 blends=screen;overlay;add;difference
set-group path=/1 blend=screen opacity=0.86
set-transform path=/1 rotate=31 pivot=600,600

# Add a diagonal spectral gradient over everything.
add-layer name=SpectralWash width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/2 type=linear from=255,30,140,120 to=40,240,255,0 from_point=0,1200 to_point=1200,0
set-layer path=/2 blend=overlay opacity=0.72

# Checker microtexture plus noise for a holographic feel.
add-layer name=Microtexture width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
checker-layer path=/3 cell=8 a=0,0,0,0 b=255,255,255,22
noise-layer path=/3 seed=7744 amount=0.42 monochrome=false
set-layer path=/3 blend=add opacity=0.30

# Mirror pass for kaleidoscope tension.
add-layer name=Mirror width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/4 file=$INPUT_IMAGE
set-layer path=/4 blend=difference opacity=0.28
set-transform path=/4 matrix=-1,0,${WIDTH},0,1,0
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
