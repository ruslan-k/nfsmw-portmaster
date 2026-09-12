#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).parents[1]
JNI = (ROOT / "runtime/src/jni_bridge.c").read_text()
HEADER = (ROOT / "runtime/src/jni_bridge.h").read_text()
MAIN = (ROOT / "runtime/src/main.c").read_text()
TRACE = (ROOT / "runtime/src/fmod_trace.c").read_text()
RELOCATION = (ROOT / "runtime/src/relocation_probe.c").read_text()
MAKEFILE = (ROOT / "runtime/Makefile").read_text()
BUILD_TSPS = (ROOT / "tools/build_tsps_runtime.sh").read_text()


def test_fmod_needshardware_patch_contract():
    assert "nfsmw_apply_fmodex_patches" in HEADER
    assert "CPU_NEEDSHARDWARE = 0x000a9b34U" in JNI
    assert "0x03a05030U, 0x0a000006U" in JNI
    assert "0xe1a00000U, 0xe1a00000U" in JNI
    call = "nfsmw_apply_fmodex_patches(&images[1U]"
    assert call in MAIN
    assert MAIN.index(call) < MAIN.index("nfsmw_apply_app_patches")


def test_fmod_music_trace_contract():
    assert "src/fmod_trace.c" in MAKEFILE
    assert '"$SRC/src/fmod_trace.c"' in BUILD_TSPS
    assert "nfsmw_fmod_trace_bind" in RELOCATION
    assert "nfsmw_fmod_trace_resolve" in RELOCATION
    # Interposition must happen before the normal guest-export lookup so the
    # app/event import is wrapped while libfmodex itself remains untouched.
    assert RELOCATION.index("nfsmw_fmod_trace_resolve") < RELOCATION.index(
        "elf32_find_export(&context->images[index], name)"
    )
    assert "_ZN4FMOD6System11createSound" in TRACE
    assert "_ZN4FMOD6System12createStream" in TRACE
    assert "_ZN4FMOD6System13setFileSystem" in TRACE
    assert 'pcs("aapcs")' in TRACE
    assert "G8-CREATE-SOUND" in TRACE
    assert "G8-CREATE-STREAM" in TRACE
    assert "G8-FS setFileSystem" in TRACE
    assert "G8-FS open" in TRACE
    assert "G8-FS async-read" in TRACE
    # The trace is diagnostic-only: every hook must delegate to the original
    # guest FMOD implementation rather than synthesize successful playback.
    assert "original_create_sound(system" in TRACE
    assert "original_create_stream(system" in TRACE
    assert "original_set_file_system(" in TRACE
    assert "original_file_open(name" in TRACE
    assert "original_file_async_read(information" in TRACE


def test_captured_fmod_signature(path):
    # The shipped ELF has p_vaddr == p_offset for the executable PT_LOAD.
    data = Path(path).read_bytes()
    words = struct.unpack_from("<II", data, 0x000A9B34)
    assert words == (0x03A05030, 0x0A000006), words


if __name__ == "__main__":
    test_fmod_needshardware_patch_contract()
    test_fmod_music_trace_contract()
    if len(sys.argv) > 1:
        test_captured_fmod_signature(sys.argv[1])
    print("FMOD audio contract: PASS")
