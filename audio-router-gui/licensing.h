#pragma once

#include "wtl.h"

class window;

class dialog_licensing : public CDialogImpl<dialog_licensing>
{
private:
    window& parent;
public:
    enum {IDD = IDD_LICENSINGDLG};

    explicit dialog_licensing(window& parent);

    BEGIN_MSG_MAP(dialog_licensing)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnBnClickedOk)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnBnClickedCancel)
    END_MSG_MAP()

    LRESULT OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};