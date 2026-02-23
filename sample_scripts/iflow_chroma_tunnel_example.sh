#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_chroma_tunnel.iflow"
RENDER_PATH="$OUT_DIR/tahoe_chroma_tunnel.png"
OPS_PATH="$OUT_DIR/tahoe_chroma_tunnel.ops"

WIDTH=1600
HEIGHT=1000
CX=800
CY=500

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
apply-effect path=/0 effect=sepia strength=0.45
set-layer path=/0 opacity=0.62

add-layer name=RedPass width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/1 file=$INPUT_IMAGE
channel-mix path=/1 rr=1 rg=0 rb=0 gr=0 gg=0 gb=0 br=0 bg=0 bb=0
set-layer path=/1 blend=add opacity=0.60
concat-transform path=/1 translate=14,2
concat-transform path=/1 scale=1.05,1.05 pivot=${CX},${CY}

add-layer name=GreenPass width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/2 file=$INPUT_IMAGE
channel-mix path=/2 rr=0 rg=0 rb=0 gr=0 gg=1 gb=0 br=0 bg=0 bb=0
set-layer path=/2 blend=add opacity=0.54
concat-transform path=/2 translate=-10,-4
concat-transform path=/2 scale=0.98,0.98 pivot=${CX},${CY}

add-layer name=BluePass width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=/3 file=$INPUT_IMAGE
channel-mix path=/3 rr=0 rg=0 rb=0 gr=0 gg=0 gb=0 br=0 bg=0 bb=1
set-layer path=/3 blend=add opacity=0.58
concat-transform path=/3 translate=4,12
concat-transform path=/3 skew=5,-4 pivot=${CX},${CY}

add-layer name=TunnelGuides width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/4 blend=screen opacity=0.52
OPS

for radius in 90 140 190 240 290 340 390 440 490; do
  echo "draw-arc path=/4 cx=${CX} cy=${CY} radius=${radius} start_deg=0 end_deg=360 rgba=60,220,255,120" >> "$OPS_PATH"
done

for deg in $(seq 0 10 350); do
  x=$(awk -v cx="$CX" -v r=560 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cx + r*cos(d*pi/180.0) }')
  y=$(awk -v cy="$CY" -v r=560 -v d="$deg" 'BEGIN { pi=atan2(0,-1); printf "%d", cy + r*sin(d*pi/180.0) }')
  echo "draw-line path=/4 x0=${CX} y0=${CY} x1=${x} y1=${y} rgba=255,120,60,85" >> "$OPS_PATH"
done

cat >> "$OPS_PATH" <<OPS
add-layer name=Atmos width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
gradient-layer path=/5 type=linear from=255,80,20,90 to=20,180,255,0 from_point=0,1000 to_point=1600,0
noise-layer path=/5 seed=777 amount=0.22 monochrome=false
set-layer path=/5 blend=overlay opacity=0.74
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
