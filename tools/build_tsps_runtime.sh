#!/usr/bin/env bash
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
SRC="$ROOT/runtime"
OUT="${OUT:-$ROOT/runtime/build/nfsmw_mapper}"
ZIG="${ZIG:-zig}"

mkdir -p "$(dirname -- "$OUT")"

runtime_sources=(
    "$SRC/src/main.c"
    "$SRC/src/elf32_loader.c"
    "$SRC/src/symbol_probe.c"
    "$SRC/src/platform_probe.c"
    "$SRC/src/relocation_probe.c"
    "$SRC/src/softfp_bridge.c"
    "$SRC/src/softfp_symbols.c"
    "$SRC/src/compat_bridge.c"
    "$SRC/src/initializer_trace.c"
    "$SRC/src/jni_bridge.c"
    "$SRC/src/obb_index.c"
    "$SRC/src/opensl_bridge.c"
    "$SRC/src/fmod_trace.c"
    "$SRC/src/crash_trace.c"
    "$SRC/src/bionic_setjmp.S"
)

# NFS guest code is ARMv7 hard-float. The host-side SDL/GLES ABI is supplied
# by the selected TSPS bridge at runtime, not linked into this executable.
"$ZIG" cc -target arm-linux-gnueabihf -mcpu=cortex_a7 \
    -O2 -s -fPIC -fPIE -fno-unwind-tables \
    -fno-asynchronous-unwind-tables -D_GNU_SOURCE \
    -D_FILE_OFFSET_BITS=64 -I "$SRC" -I "$SRC/src" \
    -o "$OUT" "${runtime_sources[@]}" -ldl -lpthread -lm

chmod 0755 "$OUT"
printf 'built NFS ARMHF runtime:\n'
file "$OUT"
