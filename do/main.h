#pragma once

#include <Windows.h>

#ifndef _WIN64
#define AUDIO_ROUTER_DLL_NAME L"audio-router.dll"
#define BOOTSTRAPPER_DLL_NAME L"bootstrapper.dll"
#else
#define AUDIO_ROUTER_DLL_NAME L"audio-router64.dll"
#define BOOTSTRAPPER_DLL_NAME L"bootstrapper64.dll"
#endif

#define CUSTOM_ERR(a) ((1 << 29) | a)