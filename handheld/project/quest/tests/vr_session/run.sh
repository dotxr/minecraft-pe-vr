#!/usr/bin/env bash
# Runs the VR layer against the Monado OpenXR runtime, headless, on the desktop:
#   apt-get install monado-service libopenxr1-monado mesa-vulkan-drivers
# Start the runtime first, with the null compositor (no window system needed) and a
# runtime directory shared with this script:
#   export XDG_RUNTIME_DIR=/tmp/vr-session-test/runtime; mkdir -p $XDG_RUNTIME_DIR
#   XRT_COMPOSITOR_NULL=1 bash -c 'sleep 100000 | monado-service' &
# OPENXR_HEADERS is the directory holding openxr/openxr.h, the same SDK version the
# Quest build fetches (see handheld/project/quest/app/src/main/cpp/CMakeLists.txt).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../../../src"
STUBS="$HERE/../gles1_shim/stub"
HEADERS="${OPENXR_HEADERS:?set OPENXR_HEADERS to a directory containing openxr/openxr.h}"
OUT="${TMPDIR:-/tmp}/vr-session-test"
mkdir -p "$OUT"

g++ -std=c++17 -O1 -w -DXR_HOST_TEST -I "$SRC" -I "$STUBS" -I "$HEADERS" \
    "$SRC/platform/vr/xr_session.cpp" "$SRC/platform/vr/vr_render.cpp" \
    "$SRC/platform/vr/vr_state.cpp" "$SRC/platform/gles1_shim.cpp" "$HERE/harness.cpp" \
    -lEGL -lGLESv2 -l:libopenxr_loader.so.1 -o "$OUT/harness"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$OUT/runtime}"
"$OUT/harness"
