#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_neon_shards.iflow"
RENDER_PATH="$OUT_DIR/tahoe_neon_shards.png"
OPS_PATH="$OUT_DIR/tahoe_neon_shards.ops"

WIDTH=1400
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
# Layer 0 is the base image from 'new --from-image'.

# Add a mirrored copy of the same image and blend it back in.
add-layer name=Mirror width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/1 file=$INPUT_IMAGE
set-layer path=/1 blend=screen opacity=0.62
set-transform path=/1 matrix=-1,0,${WIDTH},0,1,0

# Add a warm-color drift layer for a cinematic split tone.
add-layer name=SepiaDrift width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/2 file=$INPUT_IMAGE
apply-effect path=/2 effect=sepia strength=1.0
set-layer path=/2 blend=overlay opacity=0.48
set-transform path=/2 rotate=8 pivot=700,450

# Add a high-contrast monochrome layer pushed in the opposite direction.
add-layer name=MonoDrift width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
apply-effect path=/3 effect=grayscale
set-layer path=/3 blend=difference opacity=0.24
set-transform path=/3 rotate=-6 pivot=700,450

# Build a grouped field of translucent shards over the composition.
add-group name=Shards
add-grid-layers parent=/4 rows=6 cols=10 tile_width=140 tile_height=150 border=36 name_prefix=Shard opacity=0.38 fills=25,245,255,170;255,80,180,150;255,240,90,150;110,255,165,150 blends=screen;overlay;add;difference
set-group path=/4 blend=screen opacity=0.82
set-transform path=/4 rotate=-5 pivot=700,450
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
