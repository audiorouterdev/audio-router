#include "util.h"
#include <sstream>
#include <iomanip>
#include <AclAPI.h>
#include <sddl.h>

void throw_errormessage(DWORD errorcode)
{
    // TODO: add error message table resource
    if(errorcode & (1 << 29))
    {
        // custom err
        errorcode &= ~(1 << 29);
        switch(errorcode)
        {
        case 0:
            throw std::wstring(L"Invalid amount of parameters passed to audio router delegate.\n");
        case 1:
            throw std::wstring(L"Process id parameter passed to audio router delegate is invalid.\n");
        case 2:
            // currently initialization procedure covers loadlibrary & actual initialization;
            // target process might lack sufficient permissions for the procedure to start,
            // it does not contain audio functionality or file mapped parameters are invalid
            throw std::wstring(
                L"Initialization of audio routing functionality in target process failed.\n");
        case 3:
            throw std::wstring(L"Target process did not respond in time.\n");
        case 4: // TODO: obsolete
            throw std::wstring(L"Could not open target process.\n");
        default:
            throw std::wstring(L"Unknown user-defined error.\n");
        }
    }
    else
    {
        void* buffer_ptr = NULL;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | 
            FORMAT_MESSAGE_FROM_SYSTEM | 
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buffer_ptr,
            0,
            NULL);

        std::wstringstream sts;
        sts << L"Error code 0x" << std::uppercase << std::setfill(L'0') 
            << std::setw(8) << std::hex << errorcode << L": ";
        std::wstring errmsg = sts.str();
        if(buffer_ptr != NULL)
        {
            errmsg += (wchar_t*)buffer_ptr;
            LocalFree(buffer_ptr);
        }
        else
            errmsg += L". Unknown error code.\n";
        throw errmsg;
    }
}

// TODO: create security descriptor for the lowest integrity level

security_attributes::security_attributes(DWORD permissions, object_t object) : 
    everyone(NULL), packages(NULL), dacl(NULL), sacl_psd(NULL), psd(NULL), success(false)
{
    ULONG aces_count = 2;

    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa446595(v=vs.85).aspx
    DWORD sidsize = SECURITY_MAX_SID_SIZE;
    this->everyone = LocalAlloc(LMEM_FIXED, sidsize);
    CreateWellKnownSid(WinWorldSid, NULL, this->everyone, &sidsize);
    sidsize = SECURITY_MAX_SID_SIZE;
    this->packages = LocalAlloc(LMEM_FIXED, sidsize);
    if(!CreateWellKnownSid(WinBuiltinAnyPackageSid, NULL, this->packages, &sidsize))
        aces_count = 1;

    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366559(v=vs.85).aspx
    // read access to dacl is granted explicitly
    EXPLICIT_ACCESS ea[2];
    ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
    // everyone all access to file mapped object (ace)
    ea[0].grfAccessPermissions = permissions | READ_CONTROL;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = NO_INHERITANCE;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName = (LPTSTR)this->everyone;
    // app packages all access to file mapped object (ace)
    ea[1].grfAccessPermissions = permissions | READ_CONTROL;
    ea[1].grfAccessMode = SET_ACCESS;
    ea[1].grfInheritance = NO_INHERITANCE;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[1].Trustee.ptstrName = (LPTSTR)this->packages;
    // create acl that contains the aces
    if(SetEntriesInAcl(aces_count, ea, NULL, &this->dacl) != ERROR_SUCCESS)
        return;
    // initialize security descriptor
    this->psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if(!InitializeSecurityDescriptor(this->psd, SECURITY_DESCRIPTOR_REVISION))
        return;
    // add the acl to the security descriptor
    if(!SetSecurityDescriptorDacl(this->psd, TRUE, this->dacl, FALSE))
        return;

    // create low mandatory label contained sacl and set it to security descriptor
    if(object == DEFAULT)
    {
        PACL pSacl = NULL;
        LPCWSTR low_integrity_sddl_sacl = L"S:(ML;;NW;;;LW)";
        BOOL fSaclPresent = FALSE;
        BOOL fSaclDefaulted = FALSE;
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            low_integrity_sddl_sacl, SDDL_REVISION_1, &this->sacl_psd, NULL))
            return;
        // this psd is ´self-relative´ which means it manages the memory of the sacl
        if(!GetSecurityDescriptorSacl(this->sacl_psd, &fSaclPresent, &pSacl, &fSaclDefaulted))
            return;
        if(!SetSecurityDescriptorSacl(this->psd, TRUE, pSacl, FALSE))
            return;
    }

    // initialize a security attributes structure
    this->sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    this->sa.bInheritHandle = FALSE;
    this->sa.lpSecurityDescriptor = this->psd;

    this->success = true;
}
security_attributes::~security_attributes()
{
    if(this->everyone)
        FreeSid(this->everyone);
    if(this->packages)
        FreeSid(this->packages);
    if(this->dacl)
        LocalFree(this->dacl);
    if(this->sacl_psd)
        LocalFree(this->sacl_psd);
   if(this->psd)
       LocalFree(this->psd);
}