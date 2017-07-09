#pragma once

#include "dialog_main.h"
#include "formview.h"
#include "bootstrapper.h"
#include <memory>
#include <shellapi.h>

// TODO: by wolfreak99: This may not be needed
#include "clsSysTray.h"

#define WIN_WIDTH 970//400
#define WIN_HEIGHT 670//360
#define GET(type, item) reinterpret_cast<type&>(this->GetDlgItem(item))

class window : public CFrameWindowImpl<window>
{
private:
    bool dlg_main_b;

    NOTIFYICONDATA m_NotifyIconData;

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
        MESSAGE_HANDLER(WM_SYSTEMTRAYICON, OnSystemTrayIcon)
        MSG_WM_CREATE(OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_QUIT, OnQuit)
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

    enum
    {
        WM_FIRST = WM_APP,
        WM_SYSTEMTRAYICON,
    };

    int OnCreate(LPCREATESTRUCT);
    
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnQuit(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    LRESULT OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    // TODO: by wolfreak99: I believe that some similar stuff was commented out earlier relating to the NcHit, whatever that means.
    // Yeah, find out what NcHit even means, and find out if we really need to hit it.
    // I'm sure a verbal negotiation is enough.
    LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    
    LRESULT OnFileSwitchview(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnFileRefreshlist(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnFileExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    LRESULT OnSystemTrayIcon(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTrayMenuShowHide(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnTrayMenuExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    void ShowOrHideWindow();
    void ShowSystemTrayIcon();
    void HideSystemTrayIcon();
    bool IsWindowOpen();
};