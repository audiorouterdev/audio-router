#pragma once

#define PIPE_NAME L"\\\\.\\pipe\\audio-router-pipe"

#ifndef DELEGATION_ONLY_NAME

#include <Windows.h>
#include "wtl.h"

#define OUTPUT_SIZE sizeof(DWORD)
#define INPUT_SIZE (sizeof(DWORD) * 3)

class delegation
{
private:
    static DWORD WINAPI pipe_listener_proc(LPVOID);
    static void start_listen();
public:
    delegation();
    ~delegation();
};

#endif