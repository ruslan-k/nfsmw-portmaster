# NFSMW TSPS: native MP3 music diagnostic (2026-09-13)

## Request to ChatGPT

Please inspect PR #1 and implement or prescribe the smallest safe fix for **native MP3 soundtrack playback** on the TrimUI Smart Pro S. Do not use a WAV/PCM soundtrack fallback and do not disable music. Sound effects already work through the FMOD AudioTrack worker; only the FMOD music `createSound` path fails.

The requested fix must preserve:

- ARMHF guest runtime and existing EABI resolver;
- GLES bridge and 1280x720 output;
- FMOD `NEEDSHARDWARE` patch;
- audio-worker SFX path, which currently restores normal game speed;
- reversible deployment and diagnostic markers.

Please review the exact evidence below, identify the failing ABI/path boundary, and make the code changes in this PR if possible. In particular, verify whether the problem is in the FMOD `System::setFileSystem` callback ABI, OBB asset path translation, FMOD stream/createSound mode, or MP3 decoder/file reads. Do not guess from the presence of the MP3 alone.

## Physical test result

Device: TrimUI Smart Pro S / Spruce, ARM32 guest runtime with AArch64 presenter and GLES proxy.

The current physical test used:

- runtime: `nfsmw_runtime`
- launcher: `Need for Speed - Most Wanted.sh`
- `NFSMW_SILENT_AUDIO=0`
- `NFSMW_AUDIO_OUTPUT=1`
- `NFSMW_OBB_PATH` set to the canonical OBB
- no WAV files and no PCM soundtrack fallback

Observed by the user:

- in-race sound effects are audible;
- audio-worker no longer causes the earlier severe slowdown or stuttering;
- no startup/menu music;
- the one-track MP3 extraction did not change the result.

## Exact deployed hashes

```
909a6739cb990bc27266af17b7dbecd5f919f60068472ae7796aa6508e914783  nfsmw_runtime
8de48c9a3786f9679892309ec98e27fe26f39cd89d235de006c0f3e0989651ac  Need for Speed - Most Wanted.sh
90bc535dc9afb1444d38d566c5d4dcf8e2c49476793bc934863a98c144e66073  published/sounds/music/loading_01.mp3
```

Device data:

```
OBB bytes: 623470192
loading_01.mp3 bytes: 1252717
Free space: 1.6G on the game card
```

The MP3 was extracted from the OBB to:

```
/mnt/SDCARD/Roms/PORTS/nfsmw/published/sounds/music/loading_01.mp3
```

Its presence did not make native FMOD music play.

## Fresh log evidence

Log: `nfsmw/logs/nfsmw.log`, 3744286 bytes / 70189 lines after the physical run.

Audio initialization succeeds:

```
G8-SILENT user-music-override=0
G8-OPENSL PASS SDL output=24000Hz/2ch
G8-AUDIOWORKER PASS rate=24000Hz dsp-frames=1024 buffers=5 pcm-bytes=4096
G8-AUDIOWORKER mixer-running=1
```

The game reaches the music manager:

```
android[2] trace: MusicManager::SetPlaylist(): playlist=Loading fade=true
android[2] trace: MusicManager::PlayNextTrack() try, fade=true, soundManager.IsInitialized()=true, soundManager.IsUserMusicPlaying()=false
android[2] trace: MusicManager::PlayTrack(): track=published/sounds/music/loading_01.mp3 fade=true
```

That same `PlayTrack` line occurs **11170 times**. The log contains:

```
MusicManager::PlayTrack(): track=published/sounds/music/loading_01.mp3: 11170 occurrences
FMOD result of SoundManager::createSound: 16 occurrences
```

There are no successful native FMOD diagnostic markers in the current runtime:

```
G8-FS: 0
G8-CREATE-SOUND: 0
G8-CREATE-STREAM: 0
FMOD_StreamFileOpenCallback: 0
```

Representative FMOD failure:

```
android[6] Sound/SoundManager/error: FMOD result of SoundManager::createSound: An error occured that wasn't supposed to. Contact support.
android[5] Sound/SoundManager/warn: SoundManager::createSound - 'ui/ui/splash_firemonkeys': failure.
```

The generic `createSound` error is also seen for other assets, but no current hook reports the exact MP3 path, FMOD mode, callback registration, or file-open result.

## Relevant binary facts

`libapp.so` contains/imports:

```
_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE
_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i
```

`libfmodex.so` exports the corresponding FMOD 4.44 entry points, including:

```
_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE
_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE
_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i
_ZN4FMOD7SystemI19createSoundInternalEPKcjjjP22FMOD_CREATESOUNDEXINFOPPNS_4FileEbPPNS_6SoundIE
```

Relevant exported virtual/bridge addresses in the captured `libfmodex.so`:

```
System::createSound:  0x0009e0a0
System::createStream: 0x0009e058
System::setFileSystem: 0x0009ea10
System::createSoundInternal: 0x00041cc0
```

These addresses are informational only; do not hard-code them without checking the loaded ELF image and symbol/ABI contract.

## What has already been ruled out

1. **Missing MP3 file on host filesystem** — ruled out. The exact MP3 exists with a verified hash.
2. **Silent-audio configuration** — ruled out. The log reports `user-music-override=0`.
3. **Audio output startup** — ruled out for SFX. OpenSL/SDL output and the FMOD worker pass.
4. **Render-loop contention from SFX pumping** — ruled out by the worker A/B: user reports normal speed and no severe stuttering.
5. **A simple one-track extraction fix** — ruled out. The extracted MP3 did not change native music playback.

## What is not yet proven

- Whether `libapp` registers `System::setFileSystem` before the first music request.
- The exact callback ABI and calling convention used by this ARM32 FMOD build.
- Whether the callbacks receive `published/sounds/music/loading_01.mp3`, an OBB-relative path, or a transformed path.
- Whether the failure is in callback open/read/seek, `FMOD_System_CreateSound` mode flags, or MP3 codec initialization.
- Whether `MusicManager` expects `createStream` rather than `createSound` for this track.

## Requested next diagnostic/fix

Prefer one of these evidence-producing approaches, in order:

1. Add a fail-closed ARMHF wrapper around the already-resolved guest FMOD `System::setFileSystem` and log callback registration, path, open result, byte count, read sizes, seek offsets, and close result. Preserve the original callback ABI and AAPCS attributes.
2. Add a fail-closed wrapper around guest `System::createSound` and `System::createStream` that logs the original path, mode flags, result code, and output handle, without redirecting MP3 to WAV.
3. If the callback path is confirmed, fix only the path/OBB translation or callback ABI that is actually shown to fail. Do not add a broad host `fopen` substitution without proving the callback signature.

Expected success evidence should include a single native MP3 attempt with a non-error FMOD result and a music-playing marker, while SFX worker markers remain unchanged. A successful build or a present MP3 file alone is not sufficient.
