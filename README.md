# Need for Speed: Most Wanted for TrimUI Smart Pro S

A PortMaster compatibility port for the 2012 Android release of *Need for Speed: Most Wanted* (`com.ea.games.nfs13_row`, version `1.3.128`).

The port has been tested on a **TrimUI Smart Pro S running SpruceOS**. The game remains an ARMv7 guest runtime; SpruceOS provides the AArch64 presenter and GLES bridge used by the port.

This repository contains compatibility code, build scripts, documentation, and PortMaster packaging only. It does not contain the game, APK, OBB, extracted Android libraries, audio, or other Electronic Arts assets.

## Current status

- Game boots through the PortMaster menu on TrimUI Smart Pro S / SpruceOS.
- Races, menus, controller input, saves, SFX, and native MP3 soundtrack playback work.
- Music works during loading, in the front end, and during races.
- Audio uses the existing FMOD/OpenSL-to-SDL worker path for output and a native FMOD `OPENMEMORY` path for music MP3s.
- The validated display path uses the ARMv7 game/runtime plus the AArch64 GLES presenter at 1280x720 output.
- One known gameplay issue remains: the pre-race car-selection/purchase action can require the timing workaround below.

## Package

The generated package is:

```text
nfsmw-tsps-spruceos-v0.2.0.zip
```

It contains no proprietary game payload. The package includes the launcher, runtime, setup script, metadata, and compatibility files. APK/OBB files and extracted libraries are excluded.

## Installation through PortMaster

1. Copy `nfsmw-tsps-spruceos-v0.2.0.zip` to the device and install it with PortMaster.
2. Alternatively, extract the archive at the root of the SD card. The resulting port directory is:

   ```text
   /mnt/SDCARD/Roms/PORTS/nfsmw/
   ```

3. Copy your legally obtained supported game files into:

   ```text
   /mnt/SDCARD/Roms/PORTS/nfsmw/gamedata/
   ```

   Required files:

   - the supported Android APK, SHA-256 listed below;
   - `main.1003128.com.ea.games.nfs13_row.obb`, SHA-256 listed below.

4. Launch **Need for Speed: Most Wanted** from the SpruceOS/PortMaster menu. First-run setup verifies the files and extracts only the five required ARMv7 libraries from the APK. The OBB remains compressed and is read through the runtime filesystem bridge.

## Supported game files

| Item | Value |
|---|---|
| Package | `com.ea.games.nfs13_row` |
| Version | `1.3.128` (`1003128`) |
| APK SHA-256 | `bfbe9d08165b8e976924e94879b40ac6575108d5b92521ca837175c0b291c7c7` |
| OBB filename | `main.1003128.com.ea.games.nfs13_row.obb` |
| OBB SHA-256 | `66dd4e695e698929f789e7c825eabe3ba5a50ed2ce28b628c96e5dbc008043a1` |

Other releases and repacks are not supported.

## Controls

- Left stick: steering and map movement
- D-pad: menu navigation
- A: accept / drift
- B: back
- L1: brake / reverse
- R1: nitrous
- L1 / R1 in menus: change top-level section
- Start: pause / map selection
- Select: activate/deactivate the mouse cursor
- Select + Start: exit to PortMaster

### Known gameplay workaround

On the pre-race car-selection or purchase screen, press B to leave, press A to re-enter, then press A again during the short rollout window. The later modifications screen works normally with D-pad and A.

## Development

Prepare a private local test directory from your own game files:

```sh
tools/extract_nfsmw.sh \
  /path/to/your-game.apk \
  /path/to/main.1003128.com.ea.games.nfs13_row.obb \
  gamefiles
```

Build the ARMHF runtime:

```sh
podman run --rm --security-opt label=disable \
  -v "$PWD:/src:Z" docker.io/library/debian:bookworm-slim bash -lc \
  'apt-get update -qq && apt-get install -y -qq make gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf && make -C /src/runtime clean all CROSS=arm-linux-gnueabihf-'
```

Run the source contracts and package the port:

```sh
python3 tools/test_fmod_audio_contract.py path/to/libfmodex.so
python3 tools/test_tsps_av_contract.py
bash portmaster/build_port.sh
mv portmaster/dist/nfsmw.zip portmaster/dist/nfsmw-tsps-spruceos-v0.2.0.zip
```

The FMOD music fix is deliberately native MP3 playback. It does not convert the soundtrack to WAV or PCM. The runtime uses the exact FMOD Ex 4.40.06 ARM32 `FMOD_CREATESOUNDEXINFO` layout and retains per-track compressed buffers for the lifetime of the process.

## Legal boundary

The compatibility code and packaging are available under the MIT License. The license does not cover *Need for Speed*, its code, data, artwork, audio, or trademarks. Those remain the property of their respective owners.
