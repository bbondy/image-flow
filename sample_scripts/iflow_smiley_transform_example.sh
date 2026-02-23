#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="${1:-$ROOT_DIR/samples/smiley.png}"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/smiley_transform.iflow"
RENDER_PATH="$OUT_DIR/smiley_transform.png"
OPS_PATH="$OUT_DIR/smiley_transform.ops"

WIDTH=900
HEIGHT=900
CX=450
CY=450

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
set-layer path=/0 opacity=0.90
apply-effect path=/0 effect=sepia strength=0.25

add-layer name=Glow width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/1 file=$INPUT_IMAGE
apply-effect path=/1 effect=invert
gaussian-blur path=/1 radius=5 sigma=1.8
set-layer path=/1 blend=screen opacity=0.28
concat-transform path=/1 scale=1.06,1.06 pivot=${CX},${CY}

add-layer name=Orbit width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/2 file=$INPUT_IMAGE
channel-mix path=/2 rr=0 rg=0 rb=1 gr=1 gg=0 gb=0 br=0 bg=1 bb=0
set-layer path=/2 blend=overlay opacity=0.64
concat-transform path=/2 rotate=11 pivot=${CX},${CY}
concat-transform path=/2 translate=24,-20

add-layer name=Wire width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
apply-effect path=/3 effect=grayscale
edge-detect path=/3 method=sobel
apply-effect path=/3 effect=invert
levels path=/3 in_black=158 in_white=255 gamma=1.0 out_black=0 out_white=255
set-layer path=/3 blend=multiply opacity=0.46
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
