#include "delegation.h"
#include "util.h"
#include "app_list.h"
#include "app_inject.h"
#include <cassert>

delegation::delegation()
{
    start_listen();
}

delegation::~delegation()
{
}

void delegation::start_listen()
{
    HANDLE hpipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX, 
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        OUTPUT_SIZE, INPUT_SIZE, 0, 
        security_attributes(GENERIC_ALL, security_attributes::DEFAULT).get());
    if(!hpipe)
        throw_errormessage(GetLastError());
    CHandle hthr(CreateThread(NULL, 0, pipe_listener_proc, hpipe, 0, NULL));
    assert(hthr);
}

DWORD delegation::pipe_listener_proc(LPVOID arg)
{
    CHandle hpipe(arg);

    const BOOL connected = 
        ConnectNamedPipe(hpipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    start_listen();

    if(connected)
    {
        // delegate to do exe
        DWORD pid_tid_both[3];
        DWORD read;
        if(ReadFile(hpipe, &pid_tid_both, INPUT_SIZE, &read, NULL) && read == INPUT_SIZE)
        {
            DWORD exitcode = 1, written;
            try
            {
                app_list::app_info app;
                app.x86 = true; // default
                app.id = pid_tid_both[0];
                app_list::get_app_info(app, app_list::filters_t(), false);

                app_inject::inject_dll(pid_tid_both[0], app.x86, pid_tid_both[1], pid_tid_both[2]);
                exitcode = 0;
                BOOL b = WriteFile(hpipe, &exitcode, OUTPUT_SIZE, &written, NULL);
                FlushFileBuffers(hpipe);
                assert(b);
                assert(written == OUTPUT_SIZE);
            }
            catch(std::wstring err)
            {
                BOOL b = WriteFile(hpipe, &exitcode, OUTPUT_SIZE, &written, NULL);
                FlushFileBuffers(hpipe);
                assert(b);
                assert(written == OUTPUT_SIZE);

                // TODO: limit messagebox to 1 instance
                err += L"Routing functionality couldn't be initialized in the starting process.";
                MessageBox(NULL, err.c_str(), NULL, MB_ICONERROR);
            }
        }

        DisconnectNamedPipe(hpipe);
    }

    return 0;
}