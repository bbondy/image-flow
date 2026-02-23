#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_pencil_sketch.iflow"
RENDER_PATH="$OUT_DIR/tahoe_pencil_sketch.png"
OPS_PATH="$OUT_DIR/tahoe_pencil_sketch.ops"

WIDTH=1500
HEIGHT=920

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
# 0: very faint original for subtle base tone
set-layer path=/0 opacity=0.12 blend=normal

# 1: grayscale tone bed
add-layer name=ToneBase width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/1 file=$INPUT_IMAGE
apply-effect path=/1 effect=grayscale
levels path=/1 in_black=18 in_white=236 gamma=1.08 out_black=8 out_white=245
set-layer path=/1 blend=normal opacity=1.0

# 2: sketch extraction via invert + gaussian blur + color-dodge
add-layer name=DodgePass width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/2 file=$INPUT_IMAGE
apply-effect path=/2 effect=grayscale
apply-effect path=/2 effect=invert
gaussian-blur path=/2 radius=10 sigma=4.4
set-layer path=/2 blend=color-dodge opacity=0.96

# 3: hard edges for line definition
add-layer name=Edges width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
apply-effect path=/3 effect=grayscale
edge-detect path=/3 method=canny low=32 high=84
morphology path=/3 op=dilate radius=1 iterations=1
levels path=/3 in_black=120 in_white=255 gamma=1.0 out_black=0 out_white=255
set-layer path=/3 blend=multiply opacity=0.48

# 4: soft hatching from luminance
add-layer name=Hatch width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/4 file=$INPUT_IMAGE
apply-effect path=/4 effect=grayscale
hatch path=/4 spacing=9 line_width=1 ink=35,35,35,255 opacity=0.92 preserve_highlights=true
set-layer path=/4 blend=multiply opacity=0.36

# 5: paper grain/texture
add-layer name=Paper width=${WIDTH} height=${HEIGHT} fill=235,232,222,255
fractal-noise path=/5 scale=85 octaves=5 lacunarity=2.0 gain=0.55 amount=0.14 seed=4242 monochrome=true
noise-layer path=/5 seed=6001 amount=0.05 monochrome=true
curves path=/5 rgb=0,8;32,40;128,136;220,232;255,245
set-layer path=/5 blend=multiply opacity=0.50
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
