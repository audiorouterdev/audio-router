#include "licensing.h"
#include "window.h"
#include <shellapi.h>

dialog_licensing::dialog_licensing(window& parent) : parent(parent)
{
    SYSTEMTIME time;
    GetLocalTime(&time);

    if(time.wYear > 2016 || 
        (time.wYear == 2016 && (time.wMonth > 6 || (time.wMonth == 6 && time.wDay > 1))))
        this->DoModal(parent);
}

LRESULT dialog_licensing::OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if((int)ShellExecute(parent, L"open", L"https://www.reddit.com/r/software/comments/3f3em6/is_there_a_alternative_to_chevolume/cyptuz2",
        NULL, NULL, SW_SHOWNORMAL) <= 0x20)
        this->MessageBoxW(L"Could not open the web page.", NULL, MB_ICONERROR);
    this->EndDialog(0);
    return 0;
}


LRESULT dialog_licensing::OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->EndDialog(0);
    return 0;
}
