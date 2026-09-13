# Porting status

Updated: 2026-09-14

## Validated target

The current working target is the **TrimUI Smart Pro S running SpruceOS**. The game executes as an ARMv7 guest while the device uses an AArch64 GLES presenter and bridge.

| Area | Status |
|---|---|
| ARMv7 loader and relocation | Pass on TrimUI Smart Pro S / SpruceOS |
| JNI and Android compatibility | Pass for supported gameplay path |
| GLES bridge and presenter | Pass at 1280x720 output |
| Race controls | Pass |
| Front-end controls | Pass with the car-selection timing workaround |
| Sound effects | Pass |
| Native MP3 soundtrack | Pass in loading, front end, and races |
| Audio worker | Pass; no severe slowdown in the validated run |
| PortMaster package | Built and physically tested |

## Audio fix

FMOD Ex reports version `0x00044006`, which is FMOD Ex 4.40.06. Its custom async filesystem path returned `33` for every music track even though callbacks supplied valid reads. A verified memory A/B using the exact ARM32 `FMOD_CREATESOUNDEXINFO` layout and mode `0x000008c0` returned `0` and produced audible music.

The final runtime applies that native FMOD MP3 path to all files under:

```text
/published/sounds/music/*.mp3
```

Each compressed track is loaded through the already-working guest callbacks, passed to FMOD with `FMOD_SOFTWARE | FMOD_CREATESTREAM | FMOD_OPENMEMORY`, and retained until process exit. There is no WAV or PCM soundtrack fallback.

## Device verification

- Device: TrimUI Smart Pro S
- OS: SpruceOS
- Guest: ARMHF Android ARMv7 libraries
- Presenter: AArch64 GLES bridge
- Final device runtime SHA-256:
  `21d372204abd22694a82f848f3e53d3d6a20e4ba71737675a7c8087bac12488f`
- Final verified source tree: PR #1 head `d71ce374db0576af601cd4b73b954e8d96110f42`
- User-confirmed result: loading, menu, and race music all work.

## Remaining issue

The pre-race car-selection or purchase action can require pressing B to leave, A to re-enter, and A again during the short rollout window. The later modifications screen works normally.

## Reproducible checks

```sh
python3 tools/test_fmod_audio_contract.py path/to/libfmodex.so
python3 tools/test_tsps_av_contract.py
bash -n 'portmaster/Need for Speed Most Wanted.sh'
bash portmaster/build_port.sh
```

The package must not contain APK, OBB, extracted Android libraries, saves, logs, or other proprietary game payloads.
