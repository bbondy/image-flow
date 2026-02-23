#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
INPUT_IMAGE="${1:-$ROOT_DIR/samples/tahoe200-finish.webp}"
OUT_DIR="$ROOT_DIR/build/output/images"
CHANNEL_DIR="$OUT_DIR/channels"
INPUT_NAME="$(basename "$INPUT_IMAGE")"
INPUT_STEM="${INPUT_NAME%.*}"

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "Missing input image: $INPUT_IMAGE" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR" "$CHANNEL_DIR"

make_channel() {
  local channel="$1"
  local mix="$2"
  local project_path="$OUT_DIR/${INPUT_STEM}_${channel}.iflow"
  local render_path="$CHANNEL_DIR/${INPUT_STEM}_${channel}.png"

  "$BIN" new --from-image "$INPUT_IMAGE" --out "$project_path"
  "$BIN" ops --in "$project_path" --out "$project_path" --op "$mix" --render "$render_path"

  echo "Wrote channel image: $render_path"
}

make_channel "red" "channel-mix path=/0 rr=1 rg=0 rb=0 gr=0 gg=0 gb=0 br=0 bg=0 bb=0"
make_channel "green" "channel-mix path=/0 rr=0 rg=0 rb=0 gr=0 gg=1 gb=0 br=0 bg=0 bb=0"
make_channel "blue" "channel-mix path=/0 rr=0 rg=0 rb=0 gr=0 gg=0 gb=0 br=0 bg=0 bb=1"

echo "Wrote output folder: $CHANNEL_DIR"
