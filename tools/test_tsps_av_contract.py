#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).parents[1]
LAUNCHER = (ROOT / "portmaster/Need for Speed Most Wanted.sh").read_text()
JNI = (ROOT / "runtime/src/jni_bridge.c").read_text()


def test_launcher_defaults_to_panel_resolution_and_real_music_state():
    assert 'TSPGL_WIDTH="${NFSMW_WIDTH:-1280}"' in LAUNCHER
    assert 'TSPGL_HEIGHT="${NFSMW_HEIGHT:-720}"' in LAUNCHER
    assert 'NFSMW_WIDTH="${NFSMW_WIDTH:-1280}"' in LAUNCHER
    assert 'NFSMW_HEIGHT="${NFSMW_HEIGHT:-720}"' in LAUNCHER
    assert 'NFSMW_SILENT_AUDIO=${NFSMW_SILENT_AUDIO:-0}' in LAUNCHER


def test_runtime_uses_configured_dimensions_for_gles_window():
    assert 'nfsmw_platform_runtime_start(display_width, display_height)' in JNI
    assert 'NFSMW_WIDTH' in JNI
    assert 'NFSMW_HEIGHT' in JNI
    assert 'cursor_x > configured_display_width() - 9' in JNI
    assert 'cursor_y > configured_display_height() - 9' in JNI
    assert 'cursor_x > 631' not in JNI
    assert 'cursor_y > 471' not in JNI


def test_fmod_audio_runs_outside_render_loop():
    assert "struct fmod_audio_worker" in JNI
    assert "fmod_audio_worker_main" in JNI
    assert "pthread_create" in JNI
    assert "pthread_join" in JNI
    loop_start = JNI.index("for (frame = 0U; frame_limit == 0U")
    present = JNI.index("nfsmw_platform_runtime_present", loop_start)
    assert "fmod_process(" not in JNI[loop_start:present]


if __name__ == '__main__':
    test_launcher_defaults_to_panel_resolution_and_real_music_state()
    test_runtime_uses_configured_dimensions_for_gles_window()
    test_fmod_audio_runs_outside_render_loop()
    print('TSPS AV contract: PASS')
