# Spec Ops: The Line – No Sound Fix

Fixes **complete silence** in *Spec Ops: The Line* on Windows 10 and 11: no menu music, no voices, no sound effects, and silent intro videos. It's most common with USB headsets, USB DACs and PCs with several audio outputs (HDMI/DisplayPort monitors, virtual cables, VR headsets).

## Why the game goes silent

The game is 32-bit Unreal Engine 3 and uses FMOD Ex 4.32 for audio and Bink Video for movies. On modern Windows it breaks in four ways:

1. **Too many audio devices.** FMOD's old device list gets confused and sends sound to an inactive output.
2. **Surround on stereo hardware.** The engine asks for 5.1/7.1, so voices end up on speaker channels your headphones don't have.
3. **WASAPI error 61.** FMOD fails to start with `FMOD_ERR_OUTPUT_FORMAT` and gives up without a fallback.
4. **Old Bink decoder.** The shipped `binkw32.dll` (v1.99m) can't open an audio stream, so the intro videos are silent.

## How the fix works

`fmodex.dll` in this repo is a small 32-bit **proxy DLL**. The game loads it instead of the real FMOD, and it passes 695 of FMOD's 702 functions straight through to the original (renamed `fmodex_orig.dll`). The remaining calls are intercepted to:

- report a single audio device, so the game always uses your default output
- force stereo at 48 kHz, so no channel is dropped
- fall back from WASAPI to DirectSound when error 61 happens
- unmute the master volume on start
- write `fmod_mod.log` so you can see what it did

## Install

1. Open the game's `Binaries\Win32` folder (Steam: `steamapps\common\SpecOpsTheLine\Binaries\Win32`).
2. Rename the game's original `fmodex.dll` to **`fmodex_orig.dll`** (keep a backup copy too).
3. Copy `fmodex.dll` from `SpecOps_TheLine_Audio_Fix/` into that folder.
4. *(Optional, for intro video sound)* back up the game's `binkw32.dll` and replace it with the v1.99r one from this repo.
5. Start the game as usual.

To check it worked, open `fmod_mod.log` in the same folder. It should say the original DLL loaded and `DirectSound init returned 0`.

## Build it yourself

Needs 32-bit MinGW-w64 (`i686-w64-mingw32-gcc`) and the game's original FMOD DLL saved as `fmodex_orig.dll`:

```bash
gcc -m32 -shared -O2 -s -o fmodex.dll fmod_proxy.c fmodex_proxy.def -Wl,--enable-stdcall-fixup
```

`fmod_proxy.c` is the interceptor, and `fmodex_proxy.def` maps all 702 exports.

The full write-up is in [`SpecOps_TheLine_Audio_Fix/README_Audio_Fix.txt`](SpecOps_TheLine_Audio_Fix/README_Audio_Fix.txt).
