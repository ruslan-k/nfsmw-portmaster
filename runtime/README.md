# ARMHF runtime

This runtime is the ARMv7 guest component of the TrimUI Smart Pro S / SpruceOS port. The device uses an AArch64 presenter and GLES bridge; the original Android ARMv7 libraries remain in the guest address space.

The combined runtime path maps the guest libraries, inventories host symbol providers, applies ARM relocations, provides soft-float thunks, and supplies the required Bionic/Android compatibility bridges. Constructor/JNI tracing and OBB indexing are enabled by the launcher for the supported game version.

Build on an ARMHF Linux host or in the documented Debian cross-build environment:

```sh
make
build/nfsmw_mapper /path/to/gamefiles/android-libs
```

Build output is an ELF32 ARM EABI5 PIE executable with `/lib/ld-linux-armhf.so.3` as interpreter. It is not an AArch64 binary and must not be replaced with a host-native executable.

For the reproducible cross-build used for the validated device package:

```sh
podman run --rm --security-opt label=disable \
  -v "$PWD:/src:Z" docker.io/library/debian:bookworm-slim bash -lc \
  'apt-get update -qq && apt-get install -y -qq make gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf && make -C /src/runtime clean all CROSS=arm-linux-gnueabihf-'
```

The runtime includes an environment-gated read-only `/proc/cpuinfo` compatibility view. The launcher keeps `NFSMW_ARM32_CPUINFO_COMPAT=1` for the validated SpruceOS path.

## FMOD music path

The bundled FMOD Ex library reports version `0x00044006` (FMOD Ex 4.40.06). Its ARM32 `FMOD_CREATESOUNDEXINFO` is 136 bytes, with `length` at offset `0x04`. Native music uses:

```text
FMOD_SOFTWARE | FMOD_CREATESTREAM | FMOD_OPENMEMORY = 0x000008c0
```

The runtime loads all `/published/sounds/music/*.mp3` tracks through the working guest callbacks, passes each track to FMOD as compressed memory, and retains each buffer until process exit. This is native FMOD MP3 playback. No WAV or PCM soundtrack conversion is used.

## Checks

```sh
python3 tools/test_fmod_audio_contract.py path/to/libfmodex.so
python3 tools/test_tsps_av_contract.py
bash -n 'portmaster/Need for Speed Most Wanted.sh'
git diff --check
```
