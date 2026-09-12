#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).parents[1]
JNI = (ROOT / "runtime/src/jni_bridge.c").read_text()
HEADER = (ROOT / "runtime/src/jni_bridge.h").read_text()
MAIN = (ROOT / "runtime/src/main.c").read_text()


def test_fmod_needshardware_patch_contract():
    assert "nfsmw_apply_fmodex_patches" in HEADER
    assert "CPU_NEEDSHARDWARE = 0x000a9b34U" in JNI
    assert "0x03a05030U, 0x0a000006U" in JNI
    assert "0xe1a00000U, 0xe1a00000U" in JNI
    call = "nfsmw_apply_fmodex_patches(&images[1U]"
    assert call in MAIN
    assert MAIN.index(call) < MAIN.index("nfsmw_apply_app_patches")


def test_captured_fmod_signature(path):
    # The shipped ELF has p_vaddr == p_offset for the executable PT_LOAD.
    data = Path(path).read_bytes()
    words = struct.unpack_from("<II", data, 0x000A9B34)
    assert words == (0x03A05030, 0x0A000006), words


if __name__ == "__main__":
    test_fmod_needshardware_patch_contract()
    if len(sys.argv) > 1:
        test_captured_fmod_signature(sys.argv[1])
    print("FMOD NEEDSHARDWARE contract: PASS")
