================================================================================
           SPEC OPS: THE LINE - COMPLETE NO SOUND / AUDIO FIX
                   Custom FMOD Proxy & Modern Windows Guide
================================================================================

SUMMARY
-------
Many players experience complete audio silence (no intro video sound, no menu 
music, no voice dialogue, and no in-game sound effects) when playing 
"Spec Ops: The Line" on modern Windows (Windows 10 / Windows 11).

This issue is especially common for users with USB headsets, USB DACs, or modern 
multi-monitor setups with HDMI/DisplayPort audio.

This guide provides both the ready-to-use fix and a detailed explanation of 
why the bug occurs and how the custom native proxy mod resolves it.


--------------------------------------------------------------------------------
1. ROOT CAUSE ANALYSIS
--------------------------------------------------------------------------------
Spec Ops: The Line is a 32-bit (x86) Unreal Engine 3 title using:
- FMOD Ex 4.32.4 (fmodex.dll & fmod_event.dll) for game audio, music, and voices.
- RAD Game Tools Bink Video (binkw32.dll) for intro and cutscene movies.

Under modern Windows, the game suffers from four main audio failure points:

1. Device Enumeration Overflow:
   Modern PCs often have multiple audio endpoints (graphics card HDMI/DP 
   monitors, virtual audio cables, VR headsets, motherboard audio). FMOD Ex's 
   legacy DirectSound device enumeration gets confused by the list and routes 
   game audio to an inactive or disconnected endpoint.

2. Multi-Channel Channel Drop:
   Unreal Engine 3 may request surround sound (5.1 / 7.1) configurations. On 
   standard stereo headphones or USB DACs, voice dialogue and center-channel 
   effects are routed to nonexistent physical channels, resulting in silence.

3. WASAPI Format / Initialization Failure (Error 61):
   When FMOD attempts to initialize using default WASAPI with certain engine 
   buffer flags, it encounters FMOD_ERR_OUTPUT_FORMAT (error code 61). Without 
   an intelligent fallback handler, audio engine initialization fails silently.

4. Legacy Bink Video Decoder:
   The original shipping binkw32.dll (v1.99m) frequently fails to initialize 
   waveOut/DirectSound streams on Windows 10/11, rendering the intro logo 
   videos completely silent.


--------------------------------------------------------------------------------
2. HOW THE CUSTOM FMOD PROXY MOD WORKS
--------------------------------------------------------------------------------
The fix replaces "fmodex.dll" in the game directory with a lightweight, native 
32-bit proxy DLL that redirects calls to the renamed original ("fmodex_orig.dll"):

- Direct Export Forwarding:
  695 out of FMOD's 702 functions are forwarded directly to the original DLL 
  at the Windows PE loader level, ensuring zero CPU overhead and 100% binary 
  compatibility.

- Intercepted Initialization Pipeline:
  * getNumDrivers: Reports exactly 1 driver to the game engine, forcing it 
    to select the primary default active audio device (Driver 0).
  * getDriverInfo: Redirects any hardware query to Driver 0.
  * getDriverCaps: Forces speaker mode to STEREO (mode 2) and locks frequency 
    to 48 kHz, preventing surround channel-drop silence.
  * setOutput / setDriver / setSpeakerMode: Enforces stereo routing to Driver 0.
  * init: Pre-configures the pipeline and implements an automatic multi-stage 
    fallback: if WASAPI encounters error 61, it dynamically renegotiates 
    DirectSound on Driver 0.
  * Master Volume Unmute: Ensures the master channel group volume is set to 
    1.0 and unmuted upon engine start.
  * Diagnostics: Generates "fmod_mod.log" in the game directory for transparent 
    verification of device selection and return codes.


--------------------------------------------------------------------------------
3. INSTALLATION TUTORIAL (FOR PLAYERS)
--------------------------------------------------------------------------------

REQUIREMENTS:
- The modded "fmodex.dll"
- (Optional, for intro video sound) Bink Video DLL v1.99r ("binkw32.dll")

STEP 1: Locate Game Binaries Folder
Navigate to your game installation directory, inside the Win32 folder:
  <Game Directory>\Binaries\Win32\

(Example Steam path: SteamLibrary\steamapps\common\SpecOpsTheLine\Binaries\Win32)

STEP 2: Backup and Rename Original FMOD Library
1. Inside "Binaries\Win32", find the file named "fmodex.dll".
2. Make a backup copy: rename or copy it to "fmodex.dll.orig_bak".
3. Rename the original file to:
     fmodex_orig.dll
   (IMPORTANT: The proxy mod requires the original file to be named 
   fmodex_orig.dll in the same directory).

STEP 3: Install Custom Proxy DLL
Copy the modded "fmodex.dll" into:
  <Game Directory>\Binaries\Win32\fmodex.dll

STEP 4: Fix Intro Video Audio (Bink Video)
1. Find "binkw32.dll" in "Binaries\Win32".
2. Backup the original file to "binkw32.dll.orig_bak".
3. Replace "binkw32.dll" with the updated v1.99r version (available on 
   PCGamingWiki or packaged with this fix).

STEP 5: Launch the Game
Launch the game normally (via Steam or SpecOpsTheLine.exe).
Audio will now play clearly through your active headphones/speakers!

VERIFICATION:
A file named "fmod_mod.log" will be created in your "Binaries\Win32" directory. 
You can open it in Notepad to verify:
- "Successfully loaded original FMOD DLL: ...\fmodex_orig.dll"
- "System::getNumDrivers called: actual=... -> reporting 1"
- "DirectSound init returned 0"
- "Master Channel Group initialized: unmuted, volume=1.0"


--------------------------------------------------------------------------------
4. HOW TO COMPILE FROM SOURCE (FOR DEVELOPERS / MODDERS)
--------------------------------------------------------------------------------
If you prefer to build the proxy DLL yourself from source code:

REQUIREMENTS:
- 32-bit MinGW-w64 GCC (i686-w64-mingw32-gcc)

FILES:
- fmod_proxy.c (interceptor source code)
- fmodex_proxy.def (module definition file mapping all 702 exports)
- fmodex_orig.dll (original FMOD Ex DLL from the game)

BUILD COMMAND:
  gcc -m32 -shared -O2 -s -o fmodex.dll fmod_proxy.c fmodex_proxy.def -Wl,--enable-stdcall-fixup

The resulting 32-bit "fmodex.dll" is ready to drop into the game folder.


================================================================================
Enjoy Spec Ops: The Line!
================================================================================
