#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="$ROOT_DIR/samples/tahoe200-finish.webp"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/tahoe_boxes.iflow"
RENDER_PATH="$OUT_DIR/tahoe_boxes.png"
OPS_PATH="$OUT_DIR/tahoe_boxes.ops"

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

"$BIN" new --from-image "$INPUT_IMAGE" --fit "${WIDTH}x${HEIGHT}" --out "$PROJECT_PATH"

cat > "$OPS_PATH" <<'OPS'
# Add colored overlay boxes with a 10px gutter around each tile.
add-grid-layers rows=4 cols=6 border=10 name_prefix=Box opacity=0.62 fills=255,80,80,190;255,170,40,190;240,240,70,190;100,210,120,190;70,190,230,190;110,130,255,190;185,110,255,190;255,105,195,190 blends=overlay;screen;lighten;difference
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
