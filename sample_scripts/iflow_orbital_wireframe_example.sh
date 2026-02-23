#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_orbital_wireframe.iflow"
RENDER_PATH="$OUT_DIR/tahoe_orbital_wireframe.png"
OPS_PATH="$OUT_DIR/tahoe_orbital_wireframe.ops"

WIDTH=1400
HEIGHT=900
CX=700
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
apply-effect path=/0 effect=grayscale
channel-mix path=/0 rr=1.15 rg=0.10 rb=0.05 gr=0.05 gg=0.95 gb=0.05 br=0.05 bg=0.15 bb=1.15
replace-color path=/0 from=120,120,120 to=35,95,220 tolerance=90 softness=55 preserve_luma=true

add-layer name=Wire width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/1 blend=screen opacity=0.82
concat-transform path=/1 skew=8,0 pivot=${CX},${CY}
OPS

for radius in 70 105 140 175 210 245 280 315 350 385 420 455; do
  alpha=$((60 + radius / 10))
  if (( alpha > 220 )); then alpha=220; fi
  echo "draw-circle path=/1 cx=${CX} cy=${CY} radius=${radius} rgba=80,230,255,${alpha}" >> "$OPS_PATH"
done

for deg in $(seq 0 12 348); do
  x=$(awk -v cx="$CX" -v r=500 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cx + r*cos(d*pi/180.0) }')
  y=$(awk -v cy="$CY" -v r=500 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cy + r*sin(d*pi/180.0) }')
  echo "draw-line path=/1 x0=${CX} y0=${CY} x1=${x} y1=${y} rgba=255,140,60,120" >> "$OPS_PATH"
  echo "draw-fill-circle path=/1 cx=${x} cy=${y} radius=4 rgba=255,250,180,210" >> "$OPS_PATH"
done

cat >> "$OPS_PATH" <<OPS
add-layer name=Pulse width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/2 type=radial from=255,90,40,120 to=0,0,0,0 center=${CX},${CY} radius=560
noise-layer path=/2 seed=404 amount=0.24 monochrome=false
set-layer path=/2 blend=add opacity=0.44
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
