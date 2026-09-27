#include <windows.h>
#include <stdio.h>

static HMODULE g_hOrigDll = NULL;
static FILE* g_logFile = NULL;
static CRITICAL_SECTION g_logLock;
static char g_origDllPath[MAX_PATH] = {0};
static char g_logPath[MAX_PATH] = {0};

static void LogMsg(const char* fmt, ...) {
    EnterCriticalSection(&g_logLock);
    if (!g_logFile) {
        const char* path = (g_logPath[0] != '\0') ? g_logPath : "fmod_mod.log";
        g_logFile = fopen(path, "a");
    }
    if (g_logFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);
        fflush(g_logFile);
    }
    LeaveCriticalSection(&g_logLock);
}

/* Function typedefs */
typedef int (__stdcall *Fn_System_SetOutput)(void* sys, int output);
typedef int (__stdcall *Fn_System_SetDriver)(void* sys, int driver);
typedef int (__stdcall *Fn_System_Init)(void* sys, int maxchannels, int flags, void* extradriverdata);
typedef int (__stdcall *Fn_System_SetSpeakerMode)(void* sys, int mode);
typedef int (__stdcall *Fn_System_SetSoftwareFormat)(void* sys, int samplerate, int format, int numoutputchannels, int maxinputchannels, int resamplemethod);
typedef int (__stdcall *Fn_System_GetNumDrivers)(void* sys, int* numdrivers);
typedef int (__stdcall *Fn_System_GetDriverInfo)(void* sys, int id, char* name, int namelen, void* guid);
typedef int (__stdcall *Fn_System_GetDriverCaps)(void* sys, int id, unsigned int* caps, int* minfreq, int* maxfreq, int* speakermode);
typedef int (__stdcall *Fn_System_GetMasterChannelGroup)(void* sys, void** group);
typedef int (__stdcall *Fn_ChannelGroup_SetMute)(void* group, int mute);
typedef int (__stdcall *Fn_ChannelGroup_SetVolume)(void* group, float volume);

/* Function pointers */
static Fn_System_SetOutput Real_System_SetOutput = NULL;
static Fn_System_SetDriver Real_System_SetDriver = NULL;
static Fn_System_Init Real_System_Init = NULL;
static Fn_System_SetSpeakerMode Real_System_SetSpeakerMode = NULL;
static Fn_System_SetSoftwareFormat Real_System_SetSoftwareFormat = NULL;
static Fn_System_GetNumDrivers Real_System_GetNumDrivers = NULL;
static Fn_System_GetDriverInfo Real_System_GetDriverInfo = NULL;
static Fn_System_GetDriverCaps Real_System_GetDriverCaps = NULL;

static Fn_System_SetOutput Real_C_System_SetOutput = NULL;
static Fn_System_SetDriver Real_C_System_SetDriver = NULL;
static Fn_System_Init Real_C_System_Init = NULL;
static Fn_System_SetSpeakerMode Real_C_System_SetSpeakerMode = NULL;
static Fn_System_SetSoftwareFormat Real_C_System_SetSoftwareFormat = NULL;
static Fn_System_GetNumDrivers Real_C_System_GetNumDrivers = NULL;
static Fn_System_GetDriverInfo Real_C_System_GetDriverInfo = NULL;
static Fn_System_GetDriverCaps Real_C_System_GetDriverCaps = NULL;

static Fn_System_GetMasterChannelGroup Real_C_System_GetMasterChannelGroup = NULL;
static Fn_ChannelGroup_SetMute Real_C_ChannelGroup_SetMute = NULL;
static Fn_ChannelGroup_SetVolume Real_C_ChannelGroup_SetVolume = NULL;

static void InitOrigDll(void) {
    if (g_hOrigDll) return;
    const char* target = (g_origDllPath[0] != '\0') ? g_origDllPath : "fmodex_orig.dll";
    g_hOrigDll = LoadLibraryA(target);
    if (!g_hOrigDll) {
        /* Fallback to relative load */
        g_hOrigDll = LoadLibraryA("fmodex_orig.dll");
    }
    if (!g_hOrigDll) {
        LogMsg("[PROXY ERROR] Failed to load original fmodex DLL from %s! Error: %lu\n", target, GetLastError());
        return;
    }
    LogMsg("[PROXY] Successfully loaded original FMOD DLL: %s (handle: %p)\n", target, g_hOrigDll);

    /* C++ Member functions */
    Real_System_SetOutput = (Fn_System_SetOutput)GetProcAddress(g_hOrigDll, "?setOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z");
    Real_System_SetDriver = (Fn_System_SetDriver)GetProcAddress(g_hOrigDll, "?setDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
    Real_System_Init = (Fn_System_Init)GetProcAddress(g_hOrigDll, "?init@System@FMOD@@QAG?AW4FMOD_RESULT@@HIPAX@Z");
    Real_System_SetSpeakerMode = (Fn_System_SetSpeakerMode)GetProcAddress(g_hOrigDll, "?setSpeakerMode@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKERMODE@@@Z");
    Real_System_SetSoftwareFormat = (Fn_System_SetSoftwareFormat)GetProcAddress(g_hOrigDll, "?setSoftwareFormat@System@FMOD@@QAG?AW4FMOD_RESULT@@HW4FMOD_SOUND_FORMAT@@HHW4FMOD_DSP_RESAMPLER@@@Z");
    Real_System_GetNumDrivers = (Fn_System_GetNumDrivers)GetProcAddress(g_hOrigDll, "?getNumDrivers@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
    Real_System_GetDriverInfo = (Fn_System_GetDriverInfo)GetProcAddress(g_hOrigDll, "?getDriverInfo@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@@Z");
    Real_System_GetDriverCaps = (Fn_System_GetDriverCaps)GetProcAddress(g_hOrigDll, "?getDriverCaps@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAIPAH1PAW4FMOD_SPEAKERMODE@@@Z");

    /* C API functions */
    Real_C_System_SetOutput = (Fn_System_SetOutput)GetProcAddress(g_hOrigDll, "FMOD_System_SetOutput");
    Real_C_System_SetDriver = (Fn_System_SetDriver)GetProcAddress(g_hOrigDll, "FMOD_System_SetDriver");
    Real_C_System_Init = (Fn_System_Init)GetProcAddress(g_hOrigDll, "FMOD_System_Init");
    Real_C_System_SetSpeakerMode = (Fn_System_SetSpeakerMode)GetProcAddress(g_hOrigDll, "FMOD_System_SetSpeakerMode");
    Real_C_System_SetSoftwareFormat = (Fn_System_SetSoftwareFormat)GetProcAddress(g_hOrigDll, "FMOD_System_SetSoftwareFormat");
    Real_C_System_GetNumDrivers = (Fn_System_GetNumDrivers)GetProcAddress(g_hOrigDll, "FMOD_System_GetNumDrivers");
    Real_C_System_GetDriverInfo = (Fn_System_GetDriverInfo)GetProcAddress(g_hOrigDll, "FMOD_System_GetDriverInfo");
    Real_C_System_GetDriverCaps = (Fn_System_GetDriverCaps)GetProcAddress(g_hOrigDll, "FMOD_System_GetDriverCaps");

    Real_C_System_GetMasterChannelGroup = (Fn_System_GetMasterChannelGroup)GetProcAddress(g_hOrigDll, "FMOD_System_GetMasterChannelGroup");
    Real_C_ChannelGroup_SetMute = (Fn_ChannelGroup_SetMute)GetProcAddress(g_hOrigDll, "FMOD_ChannelGroup_SetMute");
    Real_C_ChannelGroup_SetVolume = (Fn_ChannelGroup_SetVolume)GetProcAddress(g_hOrigDll, "FMOD_ChannelGroup_SetVolume");
}

/* Intercepted C++ Member Functions (__stdcall ABI) */

int __stdcall Hook_System_GetNumDrivers(void* sys, int* numdrivers) {
    InitOrigDll();
    int actualDrivers = 0;
    if (Real_System_GetNumDrivers) Real_System_GetNumDrivers(sys, &actualDrivers);
    else if (Real_C_System_GetNumDrivers) Real_C_System_GetNumDrivers(sys, &actualDrivers);
    LogMsg("[HOOK] System::getNumDrivers called: actual=%d -> reporting 1 (USB Audio Device)\n", actualDrivers);
    if (numdrivers) *numdrivers = 1;
    return 0; /* FMOD_OK */
}

int __stdcall Hook_System_GetDriverInfo(void* sys, int id, char* name, int namelen, void* guid) {
    InitOrigDll();
    /* Force query Driver 0 */
    int res = Real_System_GetDriverInfo ? Real_System_GetDriverInfo(sys, 0, name, namelen, guid) :
              (Real_C_System_GetDriverInfo ? Real_C_System_GetDriverInfo(sys, 0, name, namelen, guid) : 0);
    LogMsg("[HOOK] System::getDriverInfo called (requested id=%d, queried id=0): res=%d, name=\"%s\"\n",
           id, res, name ? name : "");
    return res;
}

int __stdcall Hook_System_GetDriverCaps(void* sys, int id, unsigned int* caps, int* minfreq, int* maxfreq, int* speakermode) {
    InitOrigDll();
    int res = Real_System_GetDriverCaps ? Real_System_GetDriverCaps(sys, 0, caps, minfreq, maxfreq, speakermode) :
              (Real_C_System_GetDriverCaps ? Real_C_System_GetDriverCaps(sys, 0, caps, minfreq, maxfreq, speakermode) : 0);
    /* Enforce stereo */
    if (speakermode) *speakermode = 2; /* FMOD_SPEAKERMODE_STEREO */
    LogMsg("[HOOK] System::getDriverCaps called (req id=%d): res=%d, caps=0x%X, min=%d, max=%d, speakermode=%d\n",
           id, res, caps ? *caps : 0, minfreq ? *minfreq : 0, maxfreq ? *maxfreq : 0, speakermode ? *speakermode : 0);
    return res;
}

int __stdcall Hook_System_SetOutput(void* sys, int output) {
    InitOrigDll();
    LogMsg("[HOOK] System::setOutput called with output=%d\n", output);
    /* Force WASAPI (8) */
    int forcedOutput = 8;
    int res = Real_System_SetOutput ? Real_System_SetOutput(sys, forcedOutput) :
              (Real_C_System_SetOutput ? Real_C_System_SetOutput(sys, forcedOutput) : 0);
    LogMsg("[HOOK] System::setOutput forced to %d, result=%d\n", forcedOutput, res);
    if (res != 0) {
        /* Fallback to AUTODETECT (0) */
        res = Real_System_SetOutput ? Real_System_SetOutput(sys, 0) :
              (Real_C_System_SetOutput ? Real_C_System_SetOutput(sys, 0) : 0);
        LogMsg("[HOOK] Fallback System::setOutput(AUTODETECT), result=%d\n", res);
    }
    return res;
}

int __stdcall Hook_System_SetDriver(void* sys, int driver) {
    InitOrigDll();
    LogMsg("[HOOK] System::setDriver called with driver=%d\n", driver);
    /* Force Driver 0 (USB Audio Device) */
    int res = Real_System_SetDriver ? Real_System_SetDriver(sys, 0) :
              (Real_C_System_SetDriver ? Real_C_System_SetDriver(sys, 0) : 0);
    LogMsg("[HOOK] System::setDriver forced to 0, result=%d\n", res);
    return res;
}

int __stdcall Hook_System_SetSpeakerMode(void* sys, int mode) {
    InitOrigDll();
    LogMsg("[HOOK] System::setSpeakerMode called with mode=%d\n", mode);
    /* Force STEREO (2) */
    int res = Real_System_SetSpeakerMode ? Real_System_SetSpeakerMode(sys, 2) :
              (Real_C_System_SetSpeakerMode ? Real_C_System_SetSpeakerMode(sys, 2) : 0);
    LogMsg("[HOOK] System::setSpeakerMode forced to STEREO (2), result=%d\n", res);
    return res;
}

int __stdcall Hook_System_SetSoftwareFormat(void* sys, int samplerate, int format, int numoutputchannels, int maxinputchannels, int resamplemethod) {
    InitOrigDll();
    LogMsg("[HOOK] System::setSoftwareFormat called: rate=%d, fmt=%d, outChannels=%d, maxIn=%d, resample=%d\n",
           samplerate, format, numoutputchannels, maxinputchannels, resamplemethod);
    /* Enforce 48000 Hz, stereo (2 channels) */
    int res = Real_System_SetSoftwareFormat ? Real_System_SetSoftwareFormat(sys, 48000, format, 2, maxinputchannels, resamplemethod) :
              (Real_C_System_SetSoftwareFormat ? Real_C_System_SetSoftwareFormat(sys, 48000, format, 2, maxinputchannels, resamplemethod) : 0);
    LogMsg("[HOOK] System::setSoftwareFormat forced to 48000Hz stereo, result=%d\n", res);
    return res;
}

int __stdcall Hook_System_Init(void* sys, int maxchannels, int flags, void* extradriverdata) {
    InitOrigDll();
    LogMsg("[HOOK] System::init called: maxchannels=%d, flags=0x%X, extra=%p\n", maxchannels, flags, extradriverdata);

    /* Guarantee configuration before init */
    if (Real_System_SetOutput) Real_System_SetOutput(sys, 8);
    else if (Real_C_System_SetOutput) Real_C_System_SetOutput(sys, 8);

    if (Real_System_SetDriver) Real_System_SetDriver(sys, 0);
    else if (Real_C_System_SetDriver) Real_C_System_SetDriver(sys, 0);

    if (Real_System_SetSpeakerMode) Real_System_SetSpeakerMode(sys, 2);
    else if (Real_C_System_SetSpeakerMode) Real_C_System_SetSpeakerMode(sys, 2);

    int res = Real_System_Init ? Real_System_Init(sys, maxchannels, flags, extradriverdata) :
              (Real_C_System_Init ? Real_C_System_Init(sys, maxchannels, flags, extradriverdata) : -1);
    LogMsg("[HOOK] System::init returned %d\n", res);

    if (res != 0) {
        LogMsg("[HOOK ERROR] System::init failed (%d)! Trying DirectSound (6)...\n", res);
        if (Real_System_SetOutput) Real_System_SetOutput(sys, 6);
        else if (Real_C_System_SetOutput) Real_C_System_SetOutput(sys, 6);

        if (Real_System_SetDriver) Real_System_SetDriver(sys, 0);
        else if (Real_C_System_SetDriver) Real_C_System_SetDriver(sys, 0);

        res = Real_System_Init ? Real_System_Init(sys, maxchannels, flags, extradriverdata) :
              (Real_C_System_Init ? Real_C_System_Init(sys, maxchannels, flags, extradriverdata) : -1);
        LogMsg("[HOOK] DirectSound init returned %d\n", res);

        if (res != 0) {
            LogMsg("[HOOK ERROR] DirectSound failed (%d)! Trying AUTODETECT (0)...\n", res);
            if (Real_System_SetOutput) Real_System_SetOutput(sys, 0);
            else if (Real_C_System_SetOutput) Real_C_System_SetOutput(sys, 0);

            res = Real_System_Init ? Real_System_Init(sys, maxchannels, 0, NULL) :
                  (Real_C_System_Init ? Real_C_System_Init(sys, maxchannels, 0, NULL) : -1);
            LogMsg("[HOOK] AUTODETECT init returned %d\n", res);
        }
    }

    if (res == 0) {
        /* Ensure Master Channel Group is unmuted and volume is 1.0 */
        if (Real_C_System_GetMasterChannelGroup) {
            void* masterGroup = NULL;
            int gRes = Real_C_System_GetMasterChannelGroup(sys, &masterGroup);
            if (gRes == 0 && masterGroup) {
                if (Real_C_ChannelGroup_SetMute) Real_C_ChannelGroup_SetMute(masterGroup, 0);
                if (Real_C_ChannelGroup_SetVolume) Real_C_ChannelGroup_SetVolume(masterGroup, 1.0f);
                LogMsg("[HOOK] Master Channel Group initialized: unmuted, volume=1.0\n");
            }
        }
    }

    return res;
}

/* Intercepted C API Wrappers */

int __stdcall Hook_C_System_GetNumDrivers(void* sys, int* numdrivers) {
    return Hook_System_GetNumDrivers(sys, numdrivers);
}

int __stdcall Hook_C_System_GetDriverInfo(void* sys, int id, char* name, int namelen, void* guid) {
    return Hook_System_GetDriverInfo(sys, id, name, namelen, guid);
}

int __stdcall Hook_C_System_GetDriverCaps(void* sys, int id, unsigned int* caps, int* minfreq, int* maxfreq, int* speakermode) {
    return Hook_System_GetDriverCaps(sys, id, caps, minfreq, maxfreq, speakermode);
}

int __stdcall Hook_C_System_SetOutput(void* sys, int output) {
    return Hook_System_SetOutput(sys, output);
}

int __stdcall Hook_C_System_SetDriver(void* sys, int driver) {
    return Hook_System_SetDriver(sys, driver);
}

int __stdcall Hook_C_System_SetSpeakerMode(void* sys, int mode) {
    return Hook_System_SetSpeakerMode(sys, mode);
}

int __stdcall Hook_C_System_SetSoftwareFormat(void* sys, int samplerate, int format, int numoutputchannels, int maxinputchannels, int resamplemethod) {
    return Hook_System_SetSoftwareFormat(sys, samplerate, format, numoutputchannels, maxinputchannels, resamplemethod);
}

int __stdcall Hook_C_System_Init(void* sys, int maxchannels, int flags, void* extradriverdata) {
    return Hook_System_Init(sys, maxchannels, flags, extradriverdata);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&g_logLock);
        DisableThreadLibraryCalls(hinstDLL);

        char fullPath[MAX_PATH] = {0};
        if (GetModuleFileNameA(hinstDLL, fullPath, MAX_PATH)) {
            char* lastSlash = strrchr(fullPath, '\\');
            if (lastSlash) {
                *(lastSlash + 1) = '\0';
                snprintf(g_origDllPath, sizeof(g_origDllPath), "%sfmodex_orig.dll", fullPath);
                snprintf(g_logPath, sizeof(g_logPath), "%sfmod_mod.log", fullPath);
            }
        }

        LogMsg("====================================================\n");
        LogMsg("[FMOD PROXY] Spec Ops: The Line Audio Mod Loaded!\n");
        LogMsg("[FMOD PROXY] Module Dir: %s\n", fullPath);
        LogMsg("====================================================\n");
        InitOrigDll();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        LogMsg("[FMOD PROXY] Unloading...\n");
        if (g_logFile) {
            fclose(g_logFile);
            g_logFile = NULL;
        }
        DeleteCriticalSection(&g_logLock);
    }
    return TRUE;
}
