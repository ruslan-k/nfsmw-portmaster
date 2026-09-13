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
LAUNCHER = (ROOT / "portmaster/Need for Speed Most Wanted.sh").read_text()


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
    assert "FMOD_HARDWARE_VALUE = 0x00000020U" in TRACE
    assert "FMOD_SOFTWARE_VALUE = 0x00000040U" in TRACE
    assert "NFSMW_FMOD_MP3_SOFTWARE" in TRACE
    assert "effective_mode = (mode & ~(uint32_t)FMOD_HARDWARE_VALUE)" in TRACE
    assert "NFSMW_FMOD_MP3_CREATESTREAM" in TRACE
    assert "original_create_stream(system, name_or_data" in TRACE
    assert "effective_mode" in TRACE
    assert "stream-retry=%d" in TRACE
    assert "G8-CREATE-STREAM" in TRACE
    assert "G8-FS setFileSystem" in TRACE
    assert "G8-FS open" in TRACE
    assert "G8-FS read" in TRACE
    assert "G8-FS async-read" in TRACE
    assert "fmod_file_read_callback" in TRACE
    assert "user_read" in TRACE
    # The trace is diagnostic-only: every hook must delegate to the original
    # guest FMOD implementation rather than synthesize successful playback.
    assert "original_create_sound(system" in TRACE
    assert "original_create_stream(system" in TRACE
    assert "original_set_file_system(" in TRACE
    assert "original_file_open(name" in TRACE
    assert "original_file_read(handle" in TRACE
    assert "original_file_async_read(information" in TRACE


def test_fmod_44006_memory_ab_contract():
    compact = " ".join(TRACE.split())
    assert "struct fmod_44006_exinfo_arm32" in TRACE
    assert "FMOD_44006_EXINFO_SIZE = 136U" in TRACE
    assert "FMOD_CREATESTREAM_VALUE = 0x00000080U" in TRACE
    assert "FMOD_OPENMEMORY_VALUE = 0x00000800U" in TRACE
    assert "FMOD_MEMORY_MODE = 0x000008c0U" in TRACE
    assert "offsetof(struct fmod_44006_exinfo_arm32, length) == 0x04" in compact
    assert "offsetof(struct fmod_44006_exinfo_arm32, suggestedsoundtype) == 0x48" in compact
    assert "offsetof(struct fmod_44006_exinfo_arm32, useropen) == 0x4c" in compact
    assert "offsetof(struct fmod_44006_exinfo_arm32, initialseekposition) == 0x6c" in compact
    assert "offsetof(struct fmod_44006_exinfo_arm32, ignoresetfilesystem) == 0x74" in compact
    assert "offsetof(struct fmod_44006_exinfo_arm32, nonblockthreadid) == 0x84" in compact
    assert "NFSMW_FMOD_MP3_MEMORY_AB" in TRACE
    assert "NFSMW_FMOD_MP3_MEMORY_TARGET" in TRACE
    assert "NFSMW_FMOD_MP3_MEMORY_ALL" in TRACE
    assert '"/published/sounds/music/loading_01.mp3"' in TRACE
    assert "mp3_memory_ab_target()" in TRACE
    assert "mp3_memory_ab_source_enabled" in TRACE
    assert "FMOD_MEMORY_AB_ASSET_CAPACITY = 32U" in TRACE
    assert "FMOD_MEMORY_AB_MAX_SIZE" in TRACE
    assert "strcmp(source, mp3_memory_ab_target()) == 0" in TRACE
    assert "mode != 0x000000a0U || extra_information != NULL" in TRACE
    assert "G8-MP3-MEMAB refused" in TRACE
    assert "G8-MP3-MEMAB load-complete" in TRACE
    assert "G8-MP3-MEMAB create-begin" in TRACE
    assert "G8-MP3-MEMAB create-end" in TRACE
    assert "asset->attempted = 1" in TRACE
    assert LAUNCHER.count(
        "export NFSMW_FMOD_MP3_MEMORY_AB=${NFSMW_FMOD_MP3_MEMORY_AB:-1}"
    ) == 2
    assert LAUNCHER.count(
        "export NFSMW_FMOD_MP3_MEMORY_TARGET=${NFSMW_FMOD_MP3_MEMORY_TARGET:-/published/sounds/music/loading_01.mp3}"
    ) == 2
    assert LAUNCHER.count(
        "export NFSMW_FMOD_MP3_MEMORY_ALL=${NFSMW_FMOD_MP3_MEMORY_ALL:-1}"
    ) == 2
    assert LAUNCHER.count(
        "export NFSMW_ARM32_CPUINFO_COMPAT=${NFSMW_ARM32_CPUINFO_COMPAT:-1}"
    ) == 2
    assert "original_create_sound(system," in TRACE
    # The memory A/B is a single createSound call with explicit stream semantics;
    # the pre-existing createStream retry must be bypassed for this branch.
    memory_branch = TRACE[TRACE.index("static fmod_result try_mp3_memory_ab") :]
    memory_branch = memory_branch[: memory_branch.index("static fmod_result", 20)]
    assert "original_create_stream" not in memory_branch


def test_fmod_44006_async_completion_contract():
    compact = " ".join(TRACE.split())
    assert "struct fmod_async_read_info_44006" in TRACE
    assert "FMOD_44006_ASYNCINFO_SIZE = 32U" in TRACE
    assert "sizeof(struct fmod_async_read_info_44006) ==" in TRACE
    assert "offsetof(struct fmod_async_read_info_44006, buffer) == 0x10" in compact
    assert "offsetof(struct fmod_async_read_info_44006, bytesread) == 0x14" in compact
    assert "offsetof(struct fmod_async_read_info_44006, completion_result) == 0x18" in compact
    assert "offsetof(struct fmod_async_read_info_44006, userdata) == 0x1c" in compact
    assert "G8-FS async-read before" in TRACE
    assert "G8-FS async-read after" in TRACE
    assert "bytesread=%u completion-result=%d" in TRACE
    assert "NFSMW_FMOD_ASYNC_EOF_OK" in TRACE
    assert "G8-FS async-read eof-normalized" in TRACE
    assert LAUNCHER.count(
        "export NFSMW_FMOD_ASYNC_EOF_OK=${NFSMW_FMOD_ASYNC_EOF_OK:-1}"
    ) == 2


def test_captured_fmod_signature(path):
    # The shipped ELF has p_vaddr == p_offset for the executable PT_LOAD.
    data = Path(path).read_bytes()
    words = struct.unpack_from("<II", data, 0x000A9B34)
    assert words == (0x03A05030, 0x0A000006), words


if __name__ == "__main__":
    test_fmod_needshardware_patch_contract()
    test_fmod_music_trace_contract()
    test_fmod_44006_memory_ab_contract()
    test_fmod_44006_async_completion_contract()
    if len(sys.argv) > 1:
        test_captured_fmod_signature(sys.argv[1])
    print("FMOD audio contract: PASS")
