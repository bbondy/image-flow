#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="${1:-$ROOT_DIR/samples/smiley.png}"
VARIATION_COUNT="${2:-20}"
COPIES_PER_VARIATION="${3:-6}"
OUT_DIR="$ROOT_DIR/build/output/images"
INPUT_NAME="$(basename "$INPUT_IMAGE")"
INPUT_STEM="${INPUT_NAME%.*}"
VAR_DIR="$OUT_DIR/${INPUT_STEM}_transform_variations"
PROJECT_PATH="$VAR_DIR/${INPUT_STEM}_transform_variations.iflow"
OPS_PATH="$VAR_DIR/${INPUT_STEM}_transform_variations.ops"

WIDTH=900
HEIGHT=900
CX=450
CY=450

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "Missing input image: $INPUT_IMAGE" >&2
  exit 1
fi

if ! [[ "$VARIATION_COUNT" =~ ^[0-9]+$ ]] || [[ "$VARIATION_COUNT" -lt 1 ]]; then
  echo "Variation count must be a positive integer, got: $VARIATION_COUNT" >&2
  exit 1
fi

if ! [[ "$COPIES_PER_VARIATION" =~ ^[0-9]+$ ]] || [[ "$COPIES_PER_VARIATION" -lt 1 ]]; then
  echo "Copies per variation must be a positive integer, got: $COPIES_PER_VARIATION" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR" "$VAR_DIR"

"$BIN" new --from-image "$INPUT_IMAGE" --fit "${WIDTH}x${HEIGHT}" --out "$PROJECT_PATH"

cat > "$OPS_PATH" <<OPS
set-layer path=/0 opacity=0.82
apply-effect path=/0 effect=sepia strength=0.20
OPS

blend_modes=(screen overlay add lighten difference multiply)

for idx in $(seq 1 "$VARIATION_COUNT"); do
  tag="$(printf "%02d" "$idx")"
  group_path="/$idx"

  cat >> "$OPS_PATH" <<OPS
add-group name=variation_${tag}
OPS

  for copy in $(seq 1 "$COPIES_PER_VARIATION"); do
    layer_path="${group_path}/$((copy - 1))"
    blend="${blend_modes[$(((idx + copy - 2) % ${#blend_modes[@]}))]}"

    rot=$(( ((idx * 17 + copy * 11) % 81) - 40 ))
    tx=$(( ((idx * 37 + copy * 19) % 321) - 160 ))
    ty=$(( ((idx * 29 + copy * 23) % 321) - 160 ))
    skx=$(( ((idx * 13 + copy * 7) % 33) - 16 ))
    sky=$(( ((idx * 11 + copy * 9) % 33) - 16 ))

    scale_x=$(awk -v v="$idx" -v c="$copy" 'BEGIN { printf "%.2f", 0.72 + ((v*5 + c*4) % 57) / 100.0 }')
    scale_y=$(awk -v v="$idx" -v c="$copy" 'BEGIN { printf "%.2f", 0.72 + ((v*7 + c*3) % 57) / 100.0 }')
    opacity=$(awk -v v="$idx" -v c="$copy" 'BEGIN { printf "%.2f", 0.30 + ((v*3 + c*5) % 48) / 100.0 }')

    cat >> "$OPS_PATH" <<OPS
add-layer parent=${group_path} name=copy_${copy} width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
import-image path=${layer_path} file=$INPUT_IMAGE
set-layer path=${layer_path} blend=${blend} opacity=${opacity}
concat-transform path=${layer_path} scale=${scale_x},${scale_y} pivot=${CX},${CY}
concat-transform path=${layer_path} rotate=${rot} pivot=${CX},${CY}
concat-transform path=${layer_path} skew=${skx},${sky} pivot=${CX},${CY}
concat-transform path=${layer_path} translate=${tx},${ty}
OPS

    if (( copy % 3 == 0 )); then
      cat >> "$OPS_PATH" <<OPS
apply-effect path=${layer_path} effect=grayscale
OPS
    fi
  done

  cat >> "$OPS_PATH" <<OPS
emit file=${VAR_DIR}/${INPUT_STEM}_transform_${tag}.png
set-group path=${group_path} visible=false
OPS
done

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote ops:     $OPS_PATH"
echo "Wrote outputs: $VAR_DIR"
