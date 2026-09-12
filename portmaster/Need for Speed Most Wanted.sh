#!/bin/sh
# PORTMASTER: nfsmw.zip, Need for Speed Most Wanted.sh

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
XDG_DATA_HOME=${XDG_DATA_HOME:-${HOME:-/tmp}/.local/share}
controlfolder=
for candidate in /mnt/SDCARD/Persistent/portmaster/PortMaster \
    /opt/system/Tools/PortMaster /opt/tools/PortMaster \
    "$XDG_DATA_HOME/PortMaster" "$SCRIPT_DIR/PortMaster" /roms/ports/PortMaster; do
    if [ -f "$candidate/control.txt" ]; then
        controlfolder=$candidate
        # shellcheck disable=SC1090
        . "$controlfolder/control.txt"
        if [ -n "${CFW_NAME:-}" ] && [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
            # shellcheck disable=SC1090
            . "$controlfolder/mod_${CFW_NAME}.txt"
        fi
        command -v get_controls >/dev/null 2>&1 && get_controls
        break
    fi
done

GAMEDIR="$SCRIPT_DIR/nfsmw"
if [ ! -d "$GAMEDIR" ]; then
    for candidate in "${directory:+/${directory#/}/ports/nfsmw}" \
        /roms/ports/nfsmw /sdcard/ports/nfsmw /mnt/mmc/ports/nfsmw; do
        [ -n "$candidate" ] && [ -d "$candidate" ] && { GAMEDIR=$candidate; break; }
    done
fi
cd "$GAMEDIR" || exit 1
mkdir -p logs files cache Android/data/com.ea.games.nfs13_row || exit 1
LOG="$GAMEDIR/logs/nfsmw.log"
[ -f "$LOG" ] && mv -f -- "$LOG" "$LOG.1" 2>/dev/null || true
exec >>"$LOG" 2>&1

echo "===== nfsmw start ====="
date 2>/dev/null || true
echo "uname=$(uname -a)"
echo "platform=${PLATFORM:-unset} arch=${PLATFORM_ARCHITECTURE:-unset} cfw=${CFW_NAME:-unset}"

SETUP="$GAMEDIR/setup.sh"
LIBS="$GAMEDIR/gamefiles/android-libs"
READY="$LIBS/../.ready-1.3.128"
if [ -f "$SETUP" ]; then
    chmod +x "$SETUP" "$GAMEDIR/nfsmw_runtime" 2>/dev/null || true
    if ! "$SETUP" "$GAMEDIR"; then
        echo "NFS Most Wanted setup failed; see gamedata/README.txt"
        sleep 8
        command -v pm_finish >/dev/null 2>&1 && pm_finish
        exit 1
    fi
elif [ -s "$LIBS/libapp.so" ] && [ -f "$READY" ]; then
    echo "preprovisioned NFS runtime detected"
else
    echo "NFS Most Wanted setup.sh and preprovisioned runtime are missing"
    sleep 8
    command -v pm_finish >/dev/null 2>&1 && pm_finish
    exit 1
fi

# TrimUI Smart Pro S / spruceOS can execute the ARMv7 game/runtime, but its
# usable Mali userspace is AArch64.  The Galaxy on Fire 2 port proves a robust
# solution: keep the game/runtime in armhf and proxy GLES over a Unix socket to
# a 64-bit SDL/EGL presenter.  For early validation we intentionally reuse the
# already-installed GOF2 bridge.  A self-contained NFS copy can replace this
# dependency after the path is validated on hardware.
TSPS_BRIDGE=0
case "${NFSMW_TSPS_BRIDGE:-auto}" in
    1|yes|true|on) TSPS_BRIDGE=1 ;;
    0|no|false|off) TSPS_BRIDGE=0 ;;
    auto)
        if [ "${PLATFORM:-}" = "SmartProS" ] || \
           { [ "$(uname -m 2>/dev/null)" = "aarch64" ] && [ -d /mnt/SDCARD/spruce ]; }; then
            TSPS_BRIDGE=1
        fi
        ;;
esac

PRES=0
cleanup_presenter() {
    if [ "$PRES" -ne 0 ]; then
        kill "$PRES" 2>/dev/null || true
        wait "$PRES" 2>/dev/null || true
        PRES=0
    fi
    rm -f /tmp/nfsmw.present.ready /tmp/tsp-glbridge.sock /tmp/nfsmw.frame
}
trap cleanup_presenter EXIT INT TERM

if [ "$TSPS_BRIDGE" -eq 1 ]; then
    echo "backend=tsps-32to64-gles-bridge"

    # Prefer a self-contained NFS copy. Fall back to the working GOF2
    # installation so the bridge can still be validated independently.
    BRIDGE_ROOT=
    PRESENTER=
    for candidate in \
        "$GAMEDIR" \
        "$GAMEDIR/tsps" \
        /mnt/SDCARD/Data/ports/gof2 \
        /mnt/SDCARD/Roms/ports/gof2; do
        if [ -x "$candidate/nfsmw_present" ] && \
           [ -f "$candidate/glbridge/libEGL.so.1" ] && \
           [ -f "$candidate/armhf/lib/ld-linux-armhf.so.3" ]; then
            BRIDGE_ROOT=$candidate
            PRESENTER=$candidate/nfsmw_present
            break
        fi
        if [ -x "$candidate/gof2_present" ] && \
           [ -f "$candidate/glbridge/libEGL.so.1" ] && \
           [ -f "$candidate/armhf/lib/ld-linux-armhf.so.3" ]; then
            BRIDGE_ROOT=$candidate
            PRESENTER=$candidate/gof2_present
            break
        fi
    done
    if [ -z "$BRIDGE_ROOT" ]; then
        echo "TSPS bridge missing. Expected either:"
        echo "  $GAMEDIR/tsps/{nfsmw_present,glbridge,armhf,host-libs}"
        echo "or an installed GOF2 port at /mnt/SDCARD/Data/ports/gof2"
        echo "Set NFSMW_TSPS_BRIDGE=0 to force the legacy direct-GLES backend."
        exit 2
    fi

    LD="$BRIDGE_ROOT/armhf/lib/ld-linux-armhf.so.3"
    SYS="$BRIDGE_ROOT/armhf"
    HOST="$BRIDGE_ROOT/host-libs"
    GLBRIDGE="$BRIDGE_ROOT/glbridge"
    LIB="$GLBRIDGE:$HOST:$SYS/lib/arm-linux-gnueabihf:$SYS/lib"

    chmod a+x "$PRESENTER" "$LD" "$GAMEDIR/nfsmw_runtime" 2>/dev/null || true
    rm -f /tmp/nfsmw.present.ready /tmp/tsp-glbridge.sock /tmp/nfsmw.frame

    echo "bridge_root=$BRIDGE_ROOT"
    echo "armhf_loader=$LD"
    echo "presenter=$PRESENTER"

    # The presenter is AArch64 and owns the real Mali GLES context, window and
    # controller.  Render at the NFS native 640x480 first and letterbox it onto
    # the 1280x720 TSPS panel.  Do not optimize resolution until gameplay works.
    (
        unset LD_PRELOAD
        unset LIBGL_ALWAYS_SOFTWARE
        unset GALLIUM_DRIVER
        unset MESA_LOADER_DRIVER_OVERRIDE
        unset LIBGL_DRIVERS_PATH
        unset __EGL_VENDOR_LIBRARY_FILENAMES
        unset SDL_VIDEO_EGL_DRIVER
        export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib:/lib:/mnt/SDCARD/spruce/flip/lib"
        export SDL_VIDEO_GL_DRIVER=libGLESv2.so
        export SDL_OPENGL_ES_DRIVER=1
        export SDL_GAMECONTROLLERCONFIG_FILE="${controlfolder:+$controlfolder/gamecontrollerdb.txt}"
        export XDG_RUNTIME_DIR=/tmp
        export TMPDIR=/tmp
        export TSPGL_WIDTH="${NFSMW_WIDTH:-640}"
        export TSPGL_HEIGHT="${NFSMW_HEIGHT:-480}"
        export TSPGL_PRESENT="${NFSMW_PRESENT:-letterbox}"
        exec "$PRESENTER"
    ) &
    PRES=$!

    n=0
    while [ "$n" -lt 15 ]; do
        [ -f /tmp/nfsmw.present.ready ] && break
        kill -0 "$PRES" 2>/dev/null || break
        n=$((n + 1))
        sleep 1
    done
    echo "present_ready_wait=$n pid=$PRES"
    if [ ! -f /tmp/nfsmw.present.ready ]; then
        echo "TSPS presenter failed to become ready"
        exit 3
    fi

    export PORT_32BIT=Y
    export XDG_RUNTIME_DIR=/tmp
    export TMPDIR=/tmp
    export SDL_VIDEODRIVER=offscreen
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_VIDEO_GL_DRIVER=libGLESv2.so.2
    export SDL_VIDEO_EGL_DRIVER=libEGL.so.1
    export SDL_OPENGL_ES_DRIVER=1
    export SDL_GAMECONTROLLERCONFIG_FILE="${controlfolder:+$controlfolder/gamecontrollerdb.txt}"
    export SDL_GAMECONTROLLERCONFIG=${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}
    export SDL_NO_SIGNAL_HANDLERS=1
    export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1
    export MALLOC_ARENA_MAX=2
    export NFSMW_WIDTH="${NFSMW_WIDTH:-640}"
    export NFSMW_HEIGHT="${NFSMW_HEIGHT:-480}"
    export TSPGL_PRESENT="${NFSMW_PRESENT:-letterbox}"
    if [ -f "$HOST/libSDL2-2.0.so.0" ]; then
        export NFSMW_SDL2_LIBRARY="$HOST/libSDL2-2.0.so.0"
    fi
    unset LIBGL_ALWAYS_SOFTWARE
    unset GALLIUM_DRIVER
    unset MESA_LOADER_DRIVER_OVERRIDE
    unset LIBGL_DRIVERS_PATH
    unset __EGL_VENDOR_LIBRARY_FILENAMES
    unset EGL_PLATFORM
    unset LD_PRELOAD

    export NFSMW_RUN_CONSTRUCTORS=1 NFSMW_RUN_JNI=1 NFSMW_RUN_GAME=1
    export NFSMW_TEST_FRAMES=0
    export NFSMW_PERFORMANCE_SCORE=${NFSMW_PERFORMANCE_SCORE:-20}
    export NFSMW_SILENT_AUDIO=${NFSMW_SILENT_AUDIO:-1}
    export NFSMW_AUDIO_OUTPUT=${NFSMW_AUDIO_OUTPUT:-1}
    export NFSMW_OBB_PATH="$GAMEDIR/gamedata/main.1003128.com.ea.games.nfs13_row.obb"

    echo "armhf_library_path=$LIB"
    "$LD" --library-path "$LIB" "$GAMEDIR/nfsmw_runtime" \
        "$GAMEDIR/gamefiles/android-libs"
    result=$?
else
    echo "backend=legacy-direct-armhf-gles"
    MALI_BLOB=
    for candidate in /usr/lib/arm-linux-gnueabihf/libmali-bifrost-g31-rxp0-gbm.so \
        /usr/lib/arm-linux-gnueabihf/libMali.so \
        /usr/lib/arm-linux-gnueabihf/libmali.so.1; do
        [ -e "$candidate" ] && { MALI_BLOB=$candidate; break; }
    done
    if [ -n "$MALI_BLOB" ]; then
        GL_SHIM=/tmp/nfsmw-gl
        rm -rf -- "$GL_SHIM"
        mkdir -p "$GL_SHIM" || exit 1
        ln -sf "$MALI_BLOB" "$GL_SHIM/libEGL.so.1"
        ln -sf "$MALI_BLOB" "$GL_SHIM/libEGL.so"
        ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv2.so.2"
        ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv2.so"
        export LD_LIBRARY_PATH="$GL_SHIM${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi

    export PORT_32BIT=Y
    export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-kmsdrm}
    export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
    export SDL_VIDEO_GL_DRIVER=${SDL_VIDEO_GL_DRIVER:-libGLESv2.so}
    export SDL_VIDEO_EGL_DRIVER=${SDL_VIDEO_EGL_DRIVER:-libEGL.so}
    export SDL_GAMECONTROLLERCONFIG=${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}
    export SDL_NO_SIGNAL_HANDLERS=1
    export NFSMW_RUN_CONSTRUCTORS=1 NFSMW_RUN_JNI=1 NFSMW_RUN_GAME=1
    export NFSMW_TEST_FRAMES=0
    export NFSMW_PERFORMANCE_SCORE=${NFSMW_PERFORMANCE_SCORE:-20}
    export NFSMW_SILENT_AUDIO=${NFSMW_SILENT_AUDIO:-1}
    export NFSMW_AUDIO_OUTPUT=${NFSMW_AUDIO_OUTPUT:-1}
    export NFSMW_OBB_PATH="$GAMEDIR/gamedata/main.1003128.com.ea.games.nfs13_row.obb"

    command -v pm_platform_helper >/dev/null 2>&1 && \
        pm_platform_helper "$GAMEDIR/nfsmw_runtime"
    "$GAMEDIR/nfsmw_runtime" "$GAMEDIR/gamefiles/android-libs"
    result=$?
fi

echo "exit_code=$result"
echo "===== nfsmw end ====="
sync
cleanup_presenter
trap - EXIT INT TERM
command -v pm_finish >/dev/null 2>&1 && pm_finish
exit "$result"
