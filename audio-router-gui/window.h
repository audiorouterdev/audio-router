#pragma once

#include "dialog_main.h"
#include "formview.h"
#include "bootstrapper.h"
#include <memory>

#include "clsSysTray.h"

#define WIN_WIDTH 970//400
#define WIN_HEIGHT 670//360
#define GET(type, item) reinterpret_cast<type&>(this->GetDlgItem(item))

class window : public CFrameWindowImpl<window>
{
private:
    bool dlg_main_b;

    clsSysTray STray;
    BOOL bIsVisible;

public:
    dialog_main* dlg_main;
    formview* form_view;
#ifdef ENABLE_BOOTSTRAP
    bootstrapper* bootstrap;
#endif

    explicit window(/*bootstrapper**/);
    ~window();

    DECLARE_FRAME_WND_CLASS(L"Audio Router", IDR_MAINFRAME);

    BEGIN_MSG_MAP(window)
        MESSAGE_HANDLER(WM_SYSCOMMAND, OnSysCommand)
        MESSAGE_HANDLER(WM_TRAYNOTIFY, OnTrayNotify)
        MSG_WM_CREATE(OnCreate)
        /*MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)*/
        CHAIN_MSG_MAP(CFrameWindowImpl<window>)
        COMMAND_ID_HANDLER(ID_FILE_REFRESHLIST, OnFileRefreshlist)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAbout)
        COMMAND_ID_HANDLER(ID_FILE_SWITCHVIEW, OnFileSwitchview)
        COMMAND_ID_HANDLER(ID_FILE_EXIT, OnFileExit)
        COMMAND_ID_HANDLER(ID_TRAYMENU_SHOWHIDE, OnTrayMenuShowHide)
        COMMAND_ID_HANDLER(ID_TRAYMENU_EXIT, OnTrayMenuExit)
        /*MSG_WM_NCHITTEST(OnNcHitTest)*/
    END_MSG_MAP()

    int OnCreate(LPCREATESTRUCT);
    LRESULT OnFileRefreshlist(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnFileSwitchview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnTrayMenuShowHide(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnTrayMenuExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnTrayNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

};