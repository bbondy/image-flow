#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="${1:-$ROOT_DIR/samples/smiley.png}"
VARIATION_COUNT=20
TRANSFORM_PASSES="${2:-3}"
OUT_DIR="$ROOT_DIR/build/output/images"
INPUT_NAME="$(basename "$INPUT_IMAGE")"
INPUT_STEM="${INPUT_NAME%.*}"
VAR_DIR="$OUT_DIR/${INPUT_STEM}_transform_variations"
OPS_PATH="$VAR_DIR/${INPUT_STEM}_transform_variations.ops"
TMP_PROJECT_PATH="$(mktemp "/tmp/${INPUT_STEM}_transform_variations.XXXXXX")"

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "Missing input image: $INPUT_IMAGE" >&2
  exit 1
fi

if ! [[ "$TRANSFORM_PASSES" =~ ^[0-9]+$ ]] || [[ "$TRANSFORM_PASSES" -lt 1 ]]; then
  echo "Transform passes must be a positive integer, got: $TRANSFORM_PASSES" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR" "$VAR_DIR"
# Keep output directory image-only for this script.
find "$VAR_DIR" -maxdepth 1 -type f -name '*.iflow' -delete
trap 'rm -f "$TMP_PROJECT_PATH"' EXIT

"$BIN" new --from-image "$INPUT_IMAGE" --out "$TMP_PROJECT_PATH"

SIZE_LINE="$($BIN info --in "$TMP_PROJECT_PATH" | awk '/^Size:/{print; exit}')"
if [[ -z "$SIZE_LINE" ]]; then
  echo "Unable to determine project size from temporary project file" >&2
  exit 1
fi

SIZE_TEXT="${SIZE_LINE#Size: }"
WIDTH="${SIZE_TEXT%x*}"
HEIGHT="${SIZE_TEXT#*x}"
CX=$((WIDTH / 2))
CY=$((HEIGHT / 2))
MAX_TX=$((WIDTH / 8))
MAX_TY=$((HEIGHT / 8))

cat > "$OPS_PATH" <<OPS
set-layer path=/0 blend=normal opacity=1.0
OPS

for idx in $(seq 1 "$VARIATION_COUNT"); do
  tag="$(printf "%02d" "$idx")"

  cat >> "$OPS_PATH" <<OPS
import-image path=/0 file=$INPUT_IMAGE
set-layer path=/0 blend=normal opacity=1.0
clear-transform path=/0
OPS

  for pass in $(seq 1 "$TRANSFORM_PASSES"); do
    rot=$(( ((idx * 19 + pass * 13) % 51) - 25 ))
    skx=$(( ((idx * 11 + pass * 7) % 21) - 10 ))
    sky=$(( ((idx * 17 + pass * 5) % 21) - 10 ))

    tx=$(( ((idx * 29 + pass * 23) % (2 * MAX_TX + 1)) - MAX_TX ))
    ty=$(( ((idx * 31 + pass * 21) % (2 * MAX_TY + 1)) - MAX_TY ))

    scale_x=$(awk -v v="$idx" -v p="$pass" 'BEGIN { printf "%.2f", 0.86 + ((v*7 + p*5) % 27) / 100.0 }')
    scale_y=$(awk -v v="$idx" -v p="$pass" 'BEGIN { printf "%.2f", 0.86 + ((v*5 + p*7) % 27) / 100.0 }')

    cat >> "$OPS_PATH" <<OPS
concat-transform path=/0 scale=${scale_x},${scale_y} pivot=${CX},${CY}
concat-transform path=/0 rotate=${rot} pivot=${CX},${CY}
concat-transform path=/0 skew=${skx},${sky} pivot=${CX},${CY}
concat-transform path=/0 translate=${tx},${ty}
OPS
  done

  if (( idx % 4 == 0 )); then
    cat >> "$OPS_PATH" <<OPS
apply-effect path=/0 effect=grayscale
OPS
  fi

  if (( idx % 6 == 0 )); then
    cat >> "$OPS_PATH" <<OPS
levels path=/0 in_black=8 in_white=246 gamma=1.03 out_black=0 out_white=255
OPS
  fi

  if (( idx % 9 == 0 )); then
    cat >> "$OPS_PATH" <<OPS
apply-effect path=/0 effect=sepia strength=0.22
OPS
  fi

  cat >> "$OPS_PATH" <<OPS
emit file=${VAR_DIR}/${INPUT_STEM}_transform_${tag}.png
OPS
done

"$BIN" ops --in "$TMP_PROJECT_PATH" --out "$TMP_PROJECT_PATH" --ops-file "$OPS_PATH"

echo "Wrote ops:     $OPS_PATH"
echo "Wrote outputs: $VAR_DIR"
