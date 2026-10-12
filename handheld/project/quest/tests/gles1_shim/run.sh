#!/usr/bin/env bash
# Differential test for the GLES 1.1 shim (handheld/src/platform/gles1_shim.cpp).
#
# The same GLES 1.1 scene is rendered twice: once with desktop GL 2.1 fixed function
# (reference, same model as GLES 1.1) and once through the shim on a GLES 3 context.
# The pixel outputs must agree. Needs Mesa EGL and GL with a software driver, e.g.:
#   apt-get install libegl-dev libgles-dev libgl-dev libegl1 libgl1-mesa-dri libegl-mesa0
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SHIM="$HERE/../../../../src/platform"
OUT="${TMPDIR:-/tmp}/gles1-shim-test"
mkdir -p "$OUT"

g++ -std=c++17 -O1 -w -DREF_DESKTOP "$HERE/scene.cpp" "$HERE/ref_main.cpp" -lEGL -lGL -o "$OUT/ref"
g++ -std=c++17 -O1 -DSHIM_BUILD -w -I "$HERE/stub" -I "$SHIM" \
    "$HERE/scene.cpp" "$HERE/shim_main.cpp" "$SHIM/gles1_shim.cpp" -lEGL -o "$OUT/shim"

"$OUT/ref" "$OUT/ref.raw"
"$OUT/shim" "$OUT/shim.raw"
python3 "$HERE/compare.py" "$OUT/ref.raw" "$OUT/shim.raw"
