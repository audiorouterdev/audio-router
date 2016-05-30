#pragma once

#include <Windows.h>
#include <string>

void throw_errormessage(DWORD errorcode);

class security_attributes
{
public:
    enum object_t
    {
        DEFAULT,
        FILE_MAPPED_OBJECT
    };
private:
    bool success;
    PSID everyone, packages;
    PACL dacl;
    PSECURITY_DESCRIPTOR sacl_psd;
    PSECURITY_DESCRIPTOR psd;
    SECURITY_ATTRIBUTES sa;
public:
    // file mapped object will work at any integrity level since vista
    // defaults to no_write_up access token policy which allows for read access from
    // subjects at lower integrity level than the object;
    // mutex and pipes need low integrity mandatory label for them to work correctly
    explicit security_attributes(DWORD permissions, object_t = FILE_MAPPED_OBJECT);
    ~security_attributes();

    PSECURITY_ATTRIBUTES get() {return this->success ? &this->sa : NULL;}
};