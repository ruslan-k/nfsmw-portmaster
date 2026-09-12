# TrimUI Smart Pro S / spruceOS: 32-bit NFS MW through a 64-bit GLES presenter

## Goal

Run the existing ARMv7 Android NFS Most Wanted runtime on TrimUI Smart Pro S (A523/Mali-G57, spruceOS) without requiring a 32-bit Mali userspace.

The working Galaxy on Fire 2 port proves the architecture on the same device:

```text
ARMv7 game libraries + armhf compatibility runtime
            |
            | GLES/EGL proxy calls over Unix socket
            v
AArch64 SDL2/EGL/GLES presenter
            |
            v
64-bit Mali-G57 userspace
```

Do not rewrite the NFS Android loader/JNI/soft-float logic unless a concrete log proves it is necessary. That path already works on R36S and should remain untouched.

## Development rules for the agent

1. Work in `tsps-64bit-gles-bridge`; do not develop directly on `main`.
2. Keep the existing R36S/direct-armhf backend working. Every TSPS-specific change must be gated.
3. Prioritize the shortest route to a real hardware race. Do not spend time polishing packaging before the TSPS backend reaches gameplay.
4. Make changes in batches. Do not stop after every tiny edit to ask for confirmation.
5. Do not save or publish intermediate broken release archives. Commit source changes when they form a coherent checkpoint.
6. When a test fails, read `logs/nfsmw.log`, identify the first actual failure, patch that failure, and immediately retest. Avoid speculative rewrites.
7. Keep diagnostics concise but preserve the lines that identify: architecture, loader path, presenter readiness, GLES vendor/renderer/version, shader failures, unknown proxy opcodes, Android constructor/JNI milestones, audio startup, and exit code.
8. Stability is secondary to reaching gameplay quickly. Once races work, clean up and harden the implementation.
9. Never remove the legacy R36S code path merely because TSPS works.
10. Do not increase rendering resolution above 640x480 during initial bring-up.

## Current implementation and proof-of-concept behavior

The launcher auto-selects the bridge on spruceOS Smart Pro S. It first looks for a self-contained NFS bridge at:

```text
ports/nfsmw/tsps/
```

and otherwise reuses the already-working Galaxy on Fire 2 installation at:

```text
/mnt/SDCARD/Data/ports/gof2
```

Required GOF2-side files/directories:

```text
gof2_present
glbridge/
armhf/
host-libs/
```

The launcher also accepts a card-ready layout directly under `ports/nfsmw/`, which is the layout used by the current TSPS test. The bridge source is now in `runtime/glbridge/`; its source attribution is recorded in `runtime/glbridge/NOTICE` and `runtime/glbridge/LICENSE`. APK, OBB, extracted Android libraries, armhf sysroot and host-libs remain outside Git.

## Phase 1 — prove the bridge with the existing GOF2 runtime assets

### 1. Prepare the NFS port

Build the normal ARMv7 NFS runtime exactly as for R36S:

```bash
make -C runtime
ZIG=/path/to/zig \
TSPS_BRIDGE_ROOT=/path/to/verified/gof2/Data/ports/gof2 \
tools/build_tsps_bridge.sh
portmaster/build_port.sh
```

Install/update the resulting NFS port on the TSPS. Keep the supported NFS APK/OBB version unchanged.

The working GOF2 port must remain installed because its bridge components are temporarily reused.

### 2. First launch expectations

The log should contain:

```text
backend=tsps-32to64-gles-bridge
bridge_root=/mnt/SDCARD/Data/ports/gof2
present_ready_wait=...
armhf_loader=...
```

Then the AArch64 presenter should report a real Mali GLES context. The ARMv7 NFS runtime should run through the GOF2 armhf loader while `libEGL/libGLESv2` resolve to the proxy library.

### 3. Failure triage order

Always fix failures in this order:

#### A. `Exec format error` / armhf loader does not start

Verify:

```sh
file /mnt/SDCARD/Data/ports/gof2/armhf/lib/ld-linux-armhf.so.3
/mnt/SDCARD/Data/ports/gof2/armhf/lib/ld-linux-armhf.so.3 --help
```

GOF2 already proves ARM32 execution on this device, so this should not normally fail.

#### B. presenter does not become ready

Run the presenter manually with the same environment and inspect the log. Confirm 64-bit SDL2 is loaded from the TSPS system, not from the armhf sysroot.

Expected real graphics path:

```text
AArch64 presenter -> /usr/trimui/lib SDL/EGL/GLES -> Mali-G57
```

Do not try to install a 32-bit Mali blob.

#### C. `PLATFORM cannot load 32-bit SDL2`

The ARM32 runtime still uses SDL2 for compatibility/audio and for the EGL-facing window abstraction. Reuse the GOF2 `host-libs/libSDL2-2.0.so.0` first. Do not replace the whole runtime.

#### D. socket/proxy errors

Check for:

```text
/tmp/tsp-glbridge.sock
/tmp/nfsmw.present.ready
```

Look for `unknown op`, truncated blob, disconnect, or timeout messages. Fix only the missing/incorrect GLES operation.

#### E. shader compile/link failure

Capture the first failed shader and compare the NFS shader requirements against the GOF2 server rewriter. The bridge already contains NFS-oriented framebuffer-fetch compatibility logic, so first determine whether a GOF2-specific modification regressed NFS.

Do not rewrite all shaders globally.

#### F. black screen with successful GLES calls

Check in this order:

1. default framebuffer remapping;
2. game FBO completeness;
3. viewport/scissor state;
4. `gl_LastFragData` / framebuffer-fetch translation;
5. NPOT texture rules;
6. depth texture compare mode;
7. swap/present path.

#### G. controls fail

The first proof-of-concept may still obtain input through the ARM32 SDL path. If that is unreliable on spruceOS, move controller ownership to the AArch64 presenter exactly like GOF2 and expose buttons/axes through `nfsmw.frame`.

When implementing presenter-owned input, preserve NFS-specific button semantics and touch/cursor hacks.

#### H. sound fails

Do not block graphics bring-up on music. Preserve the existing working NFS sound-effects path first. If SDL/ALSA works but music loops, keep music disabled as in the current R36S port.

## Phase 2 — make NFS standalone after gameplay works

Only start this phase after at least one race renders and accepts input through the TSPS bridge.

### 1. Package the generic bridge

The generic bridge sources are already copied from the exact known-good GOF2 revision used for testing. Keep source attribution and document the source commit.

The relevant components are the GLES proxy client/server, protocol/op definitions and the presenter. Rename GOF2-specific output names to NFS-neutral names where practical:

```text
nfsmw_present
tsps/glbridge/libEGL.so.1
tsps/glbridge/libGLESv2.so.2
```

Do not blindly rename internal `NFSMW_*` symbols; many are already NFS-oriented and are harmless.

### 2. Build split architecture artifacts

Build:

```text
nfsmw_runtime   -> ARMv7 hard-float
proxy EGL/GLES  -> ARMv7 hard-float
nfsmw_present   -> AArch64
```

Prefer a reproducible cross build. The existing GOF2 build uses Zig successfully; reproducing that approach is acceptable.

### 3. Package a minimal armhf sysroot

After the bridge is proven, determine the actual runtime dependency set with `readelf -d` / loader diagnostics and package only the libraries needed by NFS.

Do not copy the entire GOF2 directory into the release archive if most of it is unused.

### 4. Preserve dual backend behavior

Final launcher policy:

```text
SmartProS/spruceOS -> 32->64 bridge backend
R36S/ArkOS         -> existing direct armhf Mali backend
manual override    -> NFSMW_TSPS_BRIDGE=0/1
```

Do not identify every AArch64 handheld as Smart Pro S. Keep platform detection narrow.

## Phase 3 — performance and display work

Only after stable gameplay:

1. Measure bridge overhead at native 640x480.
2. Disable excessive synchronous proxy replies where GLES semantics allow it.
3. Batch large uploads where safe.
4. Avoid per-frame heap allocation in hot paths.
5. Keep vertex/index staging persistent where possible.
6. Measure socket buffer sizing rather than guessing.
7. Test 960x720 (4:3) before 1280x720.
8. Attempt widescreen only if the game itself renders correctly at that aspect ratio.
9. Compare VSync off/on and measure frame pacing, not just average FPS.

Do not optimize by reducing correctness of texture uploads, buffer mapping, FBO state or shader compilation.

## Current hardware evidence

The card-ready NFS layout has been launched on the TSPS. The presenter reported
the real Mali-G57 GLES context and an X360 Controller, and all five NFS ARM32
modules mapped. The observed run ended with exit 143 before the constructor/game
milestones, so this is bridge bring-up evidence, not gameplay acceptance.

## Minimum acceptance criteria

A TSPS implementation is considered successful when all of the following are true:

- launcher auto-selects bridge mode on spruceOS Smart Pro S;
- ARMv7 game modules load and constructors finish;
- AArch64 presenter reports Mali-G57 GLES;
- menus render correctly;
- controller navigation works;
- a race starts;
- steering/brake/nitrous work;
- sound effects work;
- at least one race can be completed;
- exit returns cleanly to PortMaster/spruceOS;
- R36S legacy backend is still available and unchanged in behavior.

## Useful environment switches

```sh
NFSMW_TSPS_BRIDGE=1   # force bridge backend
NFSMW_TSPS_BRIDGE=0   # force legacy backend
NFSMW_WIDTH=640
NFSMW_HEIGHT=480
NFSMW_PRESENT=letterbox
NFSMW_SILENT_AUDIO=1
```

Keep 640x480 + letterbox for first bring-up.

## What not to do

- Do not search for or install a 32-bit Mali-G57 driver as the primary solution.
- Do not port the whole Android game runtime to AArch64; the proprietary game code is ARMv7.
- Do not replace the known-good ELF/JNI/Bionic loader stack without evidence.
- Do not merge TSPS hacks unconditionally into the R36S path.
- Do not start with 1280x720 rendering.
- Do not spend time building a polished release archive before the first TSPS race works.
