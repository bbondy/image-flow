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
# 0: keep faint photographic base for structure
apply-effect path=/0 effect=grayscale
levels path=/0 in_black=22 in_white=232 gamma=1.0 out_black=0 out_white=160
gaussian-blur path=/0 radius=1 sigma=0.7
set-layer path=/0 blend=multiply opacity=0.12

# 1: paper base
add-layer name=PaperBase width=${WIDTH} height=${HEIGHT} fill=246,244,238,255
set-layer path=/1 opacity=0.0

# 2: graphite midtones
add-layer name=ToneBase width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/2 file=$INPUT_IMAGE
resize-layer path=/2 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/2 effect=grayscale
gaussian-blur path=/2 radius=2 sigma=1.0
levels path=/2 in_black=24 in_white=226 gamma=1.0 out_black=0 out_white=146
curves path=/2 rgb=0,0;48,34;128,114;196,192;255,255
set-layer path=/2 blend=multiply opacity=0.45

# 3: pencil body lightening
add-layer name=DodgePass width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
resize-layer path=/3 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/3 effect=grayscale
apply-effect path=/3 effect=invert
gaussian-blur path=/3 radius=5 sigma=1.7
levels path=/3 in_black=10 in_white=240 gamma=1.0 out_black=0 out_white=255
set-layer path=/3 blend=color-dodge opacity=0.02

# 4: major contours
add-layer name=Contours width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/4 file=$INPUT_IMAGE
resize-layer path=/4 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/4 effect=grayscale
gaussian-blur path=/4 radius=1 sigma=0.8
edge-detect path=/4 method=sobel
levels path=/4 in_black=72 in_white=204 gamma=1.05 out_black=0 out_white=255
apply-effect path=/4 effect=invert
morphology path=/4 op=erode radius=1 iterations=1
set-layer path=/4 blend=multiply opacity=0.92

# 5: fine line accents
add-layer name=FineLines width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/5 file=$INPUT_IMAGE
resize-layer path=/5 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/5 effect=grayscale
edge-detect path=/5 method=canny low=44 high=118
apply-effect path=/5 effect=invert
levels path=/5 in_black=200 in_white=255 gamma=0.95 out_black=0 out_white=255
set-layer path=/5 blend=multiply opacity=0.52

# 6: coarse stroke shading
add-layer name=HatchCoarse width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/6 file=$INPUT_IMAGE
resize-layer path=/6 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/6 effect=grayscale
levels path=/6 in_black=34 in_white=214 gamma=1.12 out_black=0 out_white=255
pencil-strokes path=/6 spacing=9 length=15 thickness=1 angle=28 angle_jitter=24 jitter=2 ink=18,18,18,255 opacity=0.28 min_darkness=0.10 seed=1717
pencil-strokes path=/6 spacing=11 length=13 thickness=1 angle=-20 angle_jitter=22 jitter=2 ink=24,24,24,255 opacity=0.22 min_darkness=0.14 seed=1718
hatch path=/6 spacing=20 line_width=1 ink=30,30,30,255 opacity=1.0 preserve_highlights=true
set-layer path=/6 blend=multiply opacity=0.38

# 7: fine stroke breakup
add-layer name=HatchFine width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/7 file=$INPUT_IMAGE
resize-layer path=/7 width=${WIDTH} height=${HEIGHT} filter=bilinear
apply-effect path=/7 effect=grayscale
levels path=/7 in_black=42 in_white=224 gamma=1.08 out_black=0 out_white=255
pencil-strokes path=/7 spacing=7 length=10 thickness=1 angle=36 angle_jitter=30 jitter=1 ink=30,30,30,255 opacity=0.18 min_darkness=0.18 seed=9191
set-layer path=/7 blend=multiply opacity=0.30

# 8: pencil grain and paper texture
add-layer name=PaperTexture width=${WIDTH} height=${HEIGHT} fill=247,245,239,255
fractal-noise path=/8 scale=78 octaves=5 lacunarity=2.03 gain=0.54 amount=0.10 seed=4242 monochrome=true
noise-layer path=/8 seed=6001 amount=0.022 monochrome=true
curves path=/8 rgb=0,8;88,100;160,176;232,241;255,249
set-layer path=/8 blend=multiply opacity=0.12

# 9: global graphite veil
add-layer name=GraphiteVeil width=${WIDTH} height=${HEIGHT} fill=182,182,182,255
set-layer path=/9 blend=multiply opacity=0.18
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
