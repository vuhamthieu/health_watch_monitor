#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PY="$ROOT_DIR/tools/icon_tools/png_to_oled_c.py"
SRC_DIR="$ROOT_DIR/assets/icons/menu"
OUT_DIR="$ROOT_DIR/assets/icons/generated"

mkdir -p "$OUT_DIR"

gen() {
  local png="$1"
  local symbol="$2"
  local base="$3"
  python3 "$PY" "$SRC_DIR/$png" \
    --width 16 --height 16 \
    --name "$symbol" \
    --out-h "$OUT_DIR/${base}.h" \
    --out-c "$OUT_DIR/${base}.c"
}

gen heart.png      MICON_HEART      micon_heart
gen spo2.png       MICON_SPO2       micon_spo2
gen workout.png    MICON_WORKOUT    micon_workout
gen stopwatch.png  MICON_STOPWATCH  micon_stopwatch
gen stats.png      MICON_STATS      micon_stats
gen settings.png   MICON_SETTINGS   micon_settings

echo "Generated icons in: $OUT_DIR"
