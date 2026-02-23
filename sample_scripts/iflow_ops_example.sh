#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_boxes.iflow"
RENDER_PATH="$OUT_DIR/tahoe_boxes.png"

# Tunable output size and grid layout.
WIDTH=1200
HEIGHT=800
COLS=6
ROWS=4
BORDER=10

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "Missing input image: $INPUT_IMAGE" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR"

# Start a document, import the sample image, and resize it to a manageable canvas.
"$BIN" ops \
  --width "$WIDTH" \
  --height "$HEIGHT" \
  --out "$PROJECT_PATH" \
  --op "add-layer name=Base width=${WIDTH} height=${HEIGHT} fill=0,0,0,255" \
  --op "import-image path=/0 file=${INPUT_IMAGE}" \
  --op "resize-layer path=/0 width=${WIDTH} height=${HEIGHT} filter=bilinear"

colors=(
  "255,80,80,190"
  "255,170,40,190"
  "240,240,70,190"
  "100,210,120,190"
  "70,190,230,190"
  "110,130,255,190"
  "185,110,255,190"
  "255,105,195,190"
)

blends=(
  "overlay"
  "screen"
  "lighten"
  "difference"
)

tile_w=$((WIDTH / COLS))
tile_h=$((HEIGHT / ROWS))
inner_w=$((tile_w - (BORDER * 2)))
inner_h=$((tile_h - (BORDER * 2)))

if (( inner_w <= 0 || inner_h <= 0 )); then
  echo "Grid/border settings leave no drawable tile area" >&2
  exit 1
fi

layer_index=1
color_index=0
blend_index=0

for ((row = 0; row < ROWS; ++row)); do
  for ((col = 0; col < COLS; ++col)); do
    x=$((col * tile_w + BORDER))
    y=$((row * tile_h + BORDER))

    color="${colors[$((color_index % ${#colors[@]}))]}"
    blend="${blends[$((blend_index % ${#blends[@]}))]}"

    "$BIN" ops \
      --in "$PROJECT_PATH" \
      --out "$PROJECT_PATH" \
      --op "add-layer name=Box_${row}_${col} width=${inner_w} height=${inner_h} fill=${color}" \
      --op "set-layer path=/${layer_index} blend=${blend} opacity=0.62" \
      --op "set-layer path=/${layer_index} offset=${x},${y}"

    layer_index=$((layer_index + 1))
    color_index=$((color_index + 1))
    blend_index=$((blend_index + 1))
  done
done

"$BIN" render --in "$PROJECT_PATH" --out "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
