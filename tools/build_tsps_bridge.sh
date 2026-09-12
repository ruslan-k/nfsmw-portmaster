#!/usr/bin/env bash
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
SRC="$ROOT/runtime"
GL="$SRC/glbridge"
BUILD="$ROOT/build/tsps-bridge"
OUT="${OUT:-$ROOT/portmaster/nfsmw/tsps}"
ZIG="${ZIG:-zig}"

mkdir -p "$BUILD" "$OUT/glbridge"

common=(
    -O2 -s -fno-unwind-tables -fno-asynchronous-unwind-tables
    -I "$SRC" -I "$GL"
)

# The game-side proxy is ARMv7 hard-float. It exports both EGL and GLES2
# entry points and sends the GLES command stream to the presenter.
"$ZIG" cc -target arm-linux-gnueabihf -mcpu=cortex_a7 \
    -shared -fPIC "${common[@]}" \
    -o "$BUILD/libEGL.so.1" \
    "$GL/client.c" "$GL/client_xport.c" -lm

for name in libEGL.so libGLESv2.so libGLESv2.so.2 \
           libGLESv1_CM.so libGLESv1_CM.so.1; do
    cp -f "$BUILD/libEGL.so.1" "$OUT/glbridge/$name"
done

# The presenter is AArch64 and is the only process that opens the real SDL2 /
# EGL / Mali context. It is game-neutral; the NFS runtime remains ARMv7.
"$ZIG" cc -target aarch64-linux-gnu.2.17 \
    -O2 -s -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -I "$SRC" -I "$GL" -o "$OUT/nfsmw_present" \
    "$GL/server.c" -ldl

chmod 0755 "$OUT/nfsmw_present" "$OUT/glbridge"/*

# A standalone TSPS port also needs the ARMHF loader/sysroot and the 32-bit
# SDL2 dependency set. Keep these outside Git and copy them only when the
# caller explicitly points at the known-good GOF2 package.
if [ -n "${TSPS_BRIDGE_ROOT:-}" ]; then
    for dependency in armhf host-libs; do
        [ -d "$TSPS_BRIDGE_ROOT/$dependency" ] || {
            printf 'missing dependency directory: %s/%s\n' \
                "$TSPS_BRIDGE_ROOT" "$dependency" >&2
            exit 1
        }
        rm -rf -- "$OUT/$dependency"
        cp -a "$TSPS_BRIDGE_ROOT/$dependency" "$OUT/$dependency"
    done
fi

printf 'built bridge:\n'
file "$OUT/nfsmw_present" "$OUT/glbridge/libEGL.so.1"
