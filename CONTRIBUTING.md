# Contributing

Thanks for helping with the Need for Speed: Most Wanted compatibility port.

## Validated platform

The maintained hardware target is the TrimUI Smart Pro S running SpruceOS. The game is an ARMv7 guest runtime using the device's AArch64 GLES presenter and bridge.

## Legal boundary

Never commit, attach, or link to the game APK, OBB, extracted Android shared libraries, assets, audio, saves, or decompiled copyrighted code. Contributions should contain only independently written compatibility code, build scripts, documentation, hashes, and diagnostic logs that do not embed game payloads.

## Start here

1. Read [KNOWN_ISSUES.md](KNOWN_ISSUES.md) and [PORTING_STATUS.md](PORTING_STATUS.md).
2. Use the pinned APK/OBB hashes documented in [README.md](README.md), obtained from your own lawful copy.
3. Build the ARMHF runtime with the documented cross-toolchain.
4. Run the FMOD, AV, compatibility, soft-float, native-import, and OBB-index checks.
5. Test on a real TrimUI Smart Pro S running SpruceOS and preserve a minimal log.

## Reports and patches

Keep pull requests narrow and include:

- root cause and expected behavior;
- files and native offsets affected;
- local validation commands and results;
- real-device log, FPS, and exit state;
- confirmation that the release archive contains no proprietary payload.
