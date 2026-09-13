# TSPS NFS MW black-screen diagnostic, 2026-09-12

## Scope

This is a user-driven PortMaster launch on TrimUI Smart Pro S, not an SSH-launched game. No processes were terminated during capture.

- Device: TrimUI Smart Pro S / spruceOS
- Kernel: `Linux Longan 5.15.147 #42 SMP PREEMPT Thu Dec 18 22:31:41 CST 2025 aarch64`
- Port: NFS Most Wanted Android ARMv7 build 1.3.128
- PR: https://github.com/ruslan-k/nfsmw-portmaster/pull/1
- Launcher capture time: `2026-09-12T16:09:09 UTC`
- Diagnostic capture time: `2026-09-12T16:10:57 UTC`
- Rendering target: 640x480 game surface, letterboxed to 1280x720

## Result

The physical display was black. The launcher did not report an ELF loader error, segmentation fault or OOM. The 64-bit presenter initialized successfully, but the ARMHF runtime stopped producing progress immediately after entering provider census:

```text
platform=SmartProS arch=aarch64 cfw=TrimUI
preprovisioned NFS runtime detected
backend=tsps-32to64-gles-bridge
bridge_root=/mnt/SDCARD/Roms/PORTS/nfsmw
armhf_loader=/mnt/SDCARD/Roms/PORTS/nfsmw/armhf/lib/ld-linux-armhf.so.3
presenter=/mnt/SDCARD/Roms/PORTS/nfsmw/nfsmw_present
tspgl-srv: window 1280x720 GL context ok
tspgl-srv: GL vendor=ARM renderer=Mali-G57 version=OpenGL ES 3.2 ...
tspgl-srv: game FBO 1 color=1 depth=1 640x480 status=0x8cd5
tspgl-srv: listen /tmp/tsp-glbridge.sock
tspgl-srv: pad=Xbox 360 Controller index=0 joysticks=1
tspgl-srv: ready
present_ready_wait=1
=== G2 MAP REGRESSION ===
G2 PASS: mapped 5 ARM32 modules without guest execution
=== G3 PROVIDER CENSUS (NO GUEST EXECUTION) ===
```

No `Provider census:`, `G3-REL`, constructor, JNI or game-start line followed in the captured log. The capture found both processes still alive:

```text
presenter: sleeping in do_sys_poll, 11 threads, VmRSS=20528 kB
ARMHF loader/runtime: sleeping in hrtimer_nanosleep, 1 thread, VmRSS=12956 kB
```

The command lines were:

```text
/mnt/SDCARD/Roms/PORTS/nfsmw/nfsmw_present
/mnt/SDCARD/Roms/PORTS/nfsmw/armhf/lib/ld-linux-armhf.so.3 --library-path /mnt/SDCARD/Roms/PORTS/nfsmw/glbridge:/mnt/SDCARD/Roms/PORTS/nfsmw/host-libs:/mnt/SDCARD/Roms/PORTS/nfsmw/armhf/lib/arm-linux-gnueabihf:/mnt/SDCARD/Roms/PORTS/nfsmw/armhf/lib /mnt/SDCARD/Roms/PORTS/nfsmw/nfsmw_runtime /mnt/SDCARD/Roms/PORTS/nfsmw/gamefiles/android-libs
```

At capture time these markers existed:

```text
/tmp/nfsmw.present.ready  (3 bytes)
/tmp/tsp-glbridge.sock    (Unix socket)
/tmp/nfsmw.frame          (3686464 bytes)
```

After the user exited, the launcher cleaned the processes and markers. The observed `exit_code=143` in earlier runs is therefore consistent with external SIGTERM/cleanup after the stall, not proof of a native crash.

## Deployed artifact identities

```text
launcher                         ebf6a086a741caf88aa7ed0bcae424ea36d6630d850dae6c46ef69bfb94f865f
nfsmw_runtime                    68c2fccb7fb71238bce4a2c266be30cb223855a3a70c84d31b091982aabbf628
nfsmw_present                    eb02aa4c454573370dd88f0b2e78e84e60892e752ed336088a8782b41124cbe7
glbridge/libEGL.so.1           f0680c548fc784c1851b47ccc354df6f363aa7df0b67d1c302d13d0f039d084e7
glbridge/libGLESv2.so.2        f0680c548fc784c1851b47ccc354df6f363aa7df0b67d1c302d13d0f039d084e7
armhf/lib/ld-linux-armhf.so.3  8e92cb32a82f67783403bf4c5d9704e201fe1c15476f082655760a00205eee80
host-libs/libSDL2-2.0.so.0    7b27796080081642b85b9925d09daa541ee456e20f166c9e7753322791ab62ba
APK                              bfbe9d08165b8e976924e94879b40ac6575108d5b92521ca837175c0b291c7c7
OBB                              66dd4e695e698929f789e7c825eabe3ba5a50ed2ce28b628c96e5dbc008043a1
```

## Display and kernel evidence

- `tspgl-srv` reports a real `Mali-G57` OpenGL ES 3.2 context.
- The game FBO is complete: `status=0x8cd5` (`GL_FRAMEBUFFER_COMPLETE`).
- `modetest -p` reports an active CRTC 22 and 1280x720 CRTC dimensions.
- `/sys/class/graphics/fb0/virtual_size` is `1280,1440`.
- `/sys/class/graphics/fb0/bits_per_pixel` is `32`.
- `/sys/class/graphics/fb0/stride` is `2880`.
- `/sys/class/drm/card0-HDMI-A-1/*` is absent; this device uses the internal DSI panel, so HDMI-A-1 is not a valid connector path for this test.
- dmesg contains normal Mali initialization (`GPU identified as 0x1 arch 9.0.9 r0p1`) and no NFS-specific segfault, OOM-killer or GPU fault line during this run.

## Confirmed boundary classification

1. PortMaster launcher starts and selects the canonical NFS directory.
2. Preprovisioned NFS data is accepted without `setup.sh`.
3. AArch64 presenter starts and opens the real Mali-G57 context.
4. Controller discovery succeeds (`Xbox 360 Controller`).
5. ARMHF loader starts and maps all five ARM32 modules.
6. Failure/stall boundary is after `G2 PASS` and at the beginning of `nfsmw_probe_symbols()` / `G3 PROVIDER CENSUS`.
7. No evidence yet proves whether the provider census is looping, blocked in a dynamic-loader call, waiting on a bridge/SDL call, or being terminated by an external watchdog.
8. No evidence yet proves that a frame was submitted to the presenter, so the black screen is not yet isolated to scanout versus no-render.

## Questions for the next code review

Please inspect the PR at the linked head and the attached runtime sources before proposing a fix. Focus on the exact G3 boundary:

- Why can `nfsmw_probe_symbols()` fail to print its first provider-census result on this ARMHF/AArch64 bridge setup?
- Does opening `libEGL.so`/`libGLESv2.so` through the proxy during symbol census cause a blocking call or recursion before the bridge client is initialized?
- Is `nfsmw_compat_resolve()` safe for every imported symbol encountered during census, including the bridge/SDL symbols?
- Is the runtime process really sleeping in its own controlled wait, or is this a secondary view of a stalled thread/watchdog path?
- Does the presenter’s FBO/swap path need a first-frame/splash fallback or a bridge handshake before G3 completes?
- Should the launcher add bounded diagnostic timeouts and per-phase heartbeat markers so `143` can be classified as watchdog SIGTERM versus runtime failure?

Recommend at most three minimal tests, one changed variable per test, with expected log markers and rollback. Do not rewrite the Android loader, install a 32-bit Mali-G57 driver, or change the known-good presenter without evidence.
