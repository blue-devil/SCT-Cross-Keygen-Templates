#!/usr/bin/env bash
# Regenerate app icons from the SVG master. Run from the project root: src/icons_gen.sh
# Needs rsvg-convert (librsvg) and icotool (icoutils). Outputs are committed,
# so normal builds do not need these tools.
#   src/assets/icon/sct_<N>.png  embedded into Linux builds (_NET_WM_ICON)
#   src/assets/icon/sct.ico      Windows resource (src/sct.rc, GLFW_ICON)
set -euo pipefail
cd "$(dirname "$0")/.."

SVG=src/assets/ic_sct_brainbomb_2.svg
OUT=src/assets/icon
SIZES=(16 24 32 48 64 128 256)

mkdir -p "$OUT"
for s in "${SIZES[@]}"; do
    rsvg-convert -w "$s" -h "$s" "$SVG" -o "$OUT/sct_$s.png"
done

# 16..128 as BMP entries (widest compatibility), 256 as PNG entry (-r)
icotool -c -o "$OUT/sct.ico" \
    "$OUT"/sct_{16,24,32,48,64,128}.png -r "$OUT/sct_256.png"

echo "icons written to $OUT"
