#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_mask_etch.iflow"
RENDER_PATH="$OUT_DIR/tahoe_mask_etch.png"
OPS_PATH="$OUT_DIR/tahoe_mask_etch.ops"

WIDTH=1500
HEIGHT=900
CX=760
CY=460

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
channel-mix path=/0 rr=0.85 rg=0.10 rb=0.05 gr=0.05 gg=0.90 gb=0.05 br=0.00 bg=0.12 bb=1.05

add-layer name=Etch width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/1 file=$INPUT_IMAGE
apply-effect path=/1 effect=invert
apply-effect path=/1 effect=threshold threshold=138 lo=10,20,25,255 hi=245,245,240,255
set-layer path=/1 blend=screen opacity=0.92
mask-enable path=/1 fill=0,0,0,255
OPS

for radius in 120 170 220 270 320 370 420; do
  echo "draw-arc path=/1 target=mask cx=${CX} cy=${CY} radius=${radius} start_deg=35 end_deg=330 rgba=255,255,255,255" >> "$OPS_PATH"
done

for deg in $(seq 0 15 345); do
  x=$(awk -v cx="$CX" -v r=470 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cx + r*cos(d*pi/180.0) }')
  y=$(awk -v cy="$CY" -v r=470 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cy + r*sin(d*pi/180.0) }')
  echo "draw-line path=/1 target=mask x0=${CX} y0=${CY} x1=${x} y1=${y} rgba=255,255,255,255" >> "$OPS_PATH"
done

cat >> "$OPS_PATH" <<OPS
concat-transform path=/1 rotate=-5 pivot=${CX},${CY}

add-layer name=Shadow width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/2 type=radial from=0,0,0,0 to=0,0,0,205 center=${CX},${CY} radius=720
set-layer path=/2 blend=multiply opacity=0.84
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
