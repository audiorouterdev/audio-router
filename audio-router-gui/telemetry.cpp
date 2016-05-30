#include "telemetry.h"
#include "wtl.h"
#include <cassert>

// release:
// http://goo.gl/ZeYYSI.info
// http://goo.gl/3jWLp3.info

// 0.5 & 0.5.2 & 0.5.4 & 0.6 & 0.7 & 0.7.1 & 0.7.3 & beyond:
// (0.7.3 fixed the routing telemetry)
// http://goo.gl/7WSRWV.info
// http://goo.gl/YQqHPz.info

// 0.10:
// http://goo.gl/oeT8RN.info
// http://goo.gl/o3tD0M.info

//#define ENABLE_TELEMETRY
#ifdef _WIN64
#define DO_EXE L"do64.exe"
#else
#define DO_EXE L"do.exe"
#endif

const char telemetry::host_xor[] = {'g' ^ xor_key, 'o' ^ xor_key, 'o' ^ xor_key, 
    '.' ^ xor_key, 'g' ^ xor_key, 'l' ^ xor_key};
const char telemetry::path_xor[] = {'o' ^ xor_key, 'e' ^ xor_key, 'T' ^ xor_key, '8' ^ xor_key,
    'R' ^ xor_key, 'N' ^ xor_key};
const char telemetry::path_routed_xor[] = {'o' ^ xor_key, '3' ^ xor_key, 't' ^ xor_key,
    'D' ^ xor_key, '0' ^ xor_key, 'M' ^ xor_key};

telemetry::telemetry() : 
    times_routed(invalid_count), times_opened(invalid_count), triggered(false)
{
#ifdef ENABLE_TELEMETRY
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    this->resource_op(MAKEINTRESOURCE(101), this->times_routed);
    this->resource_op(MAKEINTRESOURCE(103), this->times_opened);

    this->update_on_open();
#endif
}

telemetry::~telemetry()
{
#ifdef ENABLE_TELEMETRY
    if(this->times_routed != invalid_count && this->times_routed <= max_count)
        this->resource_op(MAKEINTRESOURCE(101), this->times_routed, true);
    if(this->times_opened != invalid_count && this->times_opened <= max_count)
        this->resource_op(MAKEINTRESOURCE(103), this->times_opened, true);
#endif

    //WSACleanup();
}

void telemetry::xor(std::string& out, const char* data, size_t size)
{
    out.clear();
    out.reserve(size);
    for(size_t i = 0; i < size; i++)
        out += data[i] ^ xor_key;
}

void telemetry::update_on_open()
{
    // 0 stands for first time and 9 for 10th time
    if(this->times_opened == 0)
    {
        int* times_opened = new int;
        *times_opened = 0;
        CHandle hthr(CreateThread(NULL, 0, on_open_threadproc, times_opened, 0, NULL));
        assert(hthr);
    }
    else if(this->times_opened == 9)
    {
        int* times_opened = new int;
        *times_opened = 9;
        CHandle hthr(CreateThread(NULL, 0, on_open_threadproc, times_opened, 0, NULL));
        assert(hthr);
    }

    int* times_opened = new int;
    *times_opened = 1;
    CHandle hthr(CreateThread(NULL, 0, on_open_threadproc, times_opened, 0, NULL));
    assert(hthr);

    this->times_opened++;
}

void telemetry::update_on_routing()
{
    if(this->triggered)
        return;

    // 0 stands for first time and 9 for 10th time
    if(this->times_routed == 0)
    {
        int* times_routed = new int;
        *times_routed = 0;
        CHandle hthr(CreateThread(NULL, 0, on_routing_threadproc, times_routed, 0, NULL));
        assert(hthr);
    }
    else if(this->times_routed == 9)
    {
        int* times_routed = new int;
        *times_routed = 9;
        CHandle hthr(CreateThread(NULL, 0, on_routing_threadproc, times_routed, 0, NULL));
        assert(hthr);
    }

    int* times_routed = new int;
    *times_routed = 1;
    CHandle hthr(CreateThread(NULL, 0, on_routing_threadproc, times_routed, 0, NULL));
    assert(hthr);

    this->times_routed++;

    this->triggered = true;
}

SOCKET telemetry::try_connect(std::string& url)
{
    int res;
    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // resolve address
    addrinfo* result;
    xor(url, host_xor, sizeof(host_xor));
    res = getaddrinfo(url.c_str(), "80", &hints, &result);
    if(res != 0)
        return INVALID_SOCKET;

    // try connecting
    addrinfo* ptr;
    SOCKET sock = INVALID_SOCKET;
    for(ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if(sock == INVALID_SOCKET)
        {
            freeaddrinfo(result);
            return INVALID_SOCKET;
        }

        res = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if(res == SOCKET_ERROR)
        {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    return sock;
}

void telemetry::try_send(
    SOCKET sock, const std::string& host, const std::string& path, const std::string& tm)
{
    std::string request;
    request = "GET /";
    request += path;
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += "\r\nUser-Agent: Mozilla/5.0 (en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) ";
    request += tm;
    request += "/7B405";
    request += "\r\nConnection: close\r\n\r\n";
    int res = send(sock, request.c_str(), (int)request.length(), 0);
    assert(res != SOCKET_ERROR);
    char buf[512];
    for(;res != SOCKET_ERROR;)
    {
        if(recv(sock, buf, sizeof(buf), 0) <= 0)
            break;
    }

    memset(buf, 0, sizeof(buf));
    request.clear();
}

DWORD telemetry::on_open_threadproc(LPVOID param)
{
    const int times_opened = *((int*)param);
    delete param;

    std::string host;
    SOCKET sock = try_connect(host);
    if(sock == INVALID_SOCKET)
    {
        host.clear();
        return 0;
    }

    std::string path;
    xor(path, path_xor, sizeof(path_xor));

    if(times_opened == 0)
        try_send(sock, host, path, "opened_0");
    else if(times_opened == 9)
        try_send(sock, host, path, "opened_9");
    else
        try_send(sock, host, path, "opened");

    path.clear();
    host.clear();
    closesocket(sock);
    return 0;
}

DWORD telemetry::on_routing_threadproc(LPVOID param)
{
    const int times_routed = *((int*)param);
    delete param;

    std::string host;
    SOCKET sock = try_connect(host);
    if(sock == INVALID_SOCKET)
    {
        host.clear();
        return 0;
    }

    std::string path;
    xor(path, path_routed_xor, sizeof(path_routed_xor));

    if(times_routed == 0)
        try_send(sock, host, path, "routed_0");
    else if(times_routed == 9)
        try_send(sock, host, path, "routed_9");
    else
        try_send(sock, host, path, "routed");

    path.clear();
    host.clear();
    closesocket(sock);
    return 0;
}

void telemetry::resource_op(LPCWSTR name, int& in, bool write)
{
    if(!write)
    {
        HMODULE hexe = LoadLibrary(DO_EXE);
        HRSRC hres = FindResource(hexe, name, MAKEINTRESOURCE(256));
        if(hres != NULL)
        {
            LPVOID lock = LockResource(LoadResource(hexe, hres));
            if(lock != NULL)
            {
                DWORD count = *((DWORD*)lock);
                in = (int)count;
            }
        }
        FreeLibrary(hexe);
    }
    else
    {
        DWORD count = (DWORD)in;
        // beginupdateresource does not work in debugging context
        HANDLE hupdateres = BeginUpdateResource(DO_EXE, FALSE);
        BOOL res = UpdateResource(
            hupdateres, MAKEINTRESOURCE(256), name,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &count, sizeof(DWORD));
        assert(res);
        res = EndUpdateResource(hupdateres, FALSE);
        assert(res);
    }
}