#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

shopt -s nullglob
scripts=("$SCRIPT_DIR"/iflow_*_example.sh)
shopt -u nullglob

if [[ ${#scripts[@]} -eq 0 ]]; then
  echo "No sample scripts found in $SCRIPT_DIR" >&2
  exit 1
fi

for script in "${scripts[@]}"; do
  echo "==> Running $(basename "$script")"
  "$script"
  echo
done

echo "All sample scripts completed successfully."
