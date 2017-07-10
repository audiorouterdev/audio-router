#include "window.h"

#ifndef DISABLE_TELEMETRY
telemetry* telemetry_m = NULL;
#endif

// TODO: by wolfreak99: Check audiorouterdevs "remove saved routing functionality"
// To determine if bootstrap (and telemetry) are related to feature..
#ifdef ENABLE_BOOTSTRAP
window::window(bootstrapper* bootstrap) : dlg_main_b(true), bootstrap(bootstrap)
#else
window::window(/*bootstrapper* bootstrap*/) : dlg_main_b(true)/*, bootstrap(bootstrap)*/
#endif
{
    this->dlg_main = new dialog_main(*this);
    this->form_view = new formview(*this);
}

window::~window()
{
    if (this->dlg_main_b)
        delete this->dlg_main;
    delete this->form_view;
    
#ifndef DISABLE_TELEMETRY
    delete telemetry_m;
    telemetry_m = NULL;
#endif
}

int window::OnCreate(LPCREATESTRUCT lpcs)
{
#ifndef DISABLE_TELEMETRY
    telemetry_m = new telemetry;
#endif

    this->m_hWndClient = this->dlg_main->Create(this->m_hWnd);
    this->dlg_main->ShowWindow(SW_SHOW);

    ShowSystemTrayIcon();

    return 0;
}

LRESULT window::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    HideSystemTrayIcon();
    return 0;
}

LRESULT window::OnQuit(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->DestroyWindow();
    // TODO: by wolfreak99: This may not even be needed
    PostQuitMessage(0);
    return 0;
}

LRESULT window::OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return HTCLOSE;
}

LRESULT window::OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    bHandled = FALSE;
    // TODO: by wolfreak99: does "bHandled" act the same way as unityengines event eating?
    // Does this being true basically mean the system won't handle the event afterward, and
    // it being false mean it would handle it? if so, consider utilizing it when ensuring
    // the tray menu exit closes properly..

    switch (wParam) {
    case SC_CLOSE:
        // Closing the window shows the icon while hiding it from the minimized windows bar.
        this->ShowWindow(SW_HIDE);
        bHandled = TRUE;
        break;
    case SC_RESTORE:
        // TODO: by wolfreak99: This iteration may not be needed, as it's commented out
        // as well in the aformentioned cherry-pick. i'm keeping it here for now just in case
        for (dialog_main::dialog_arrays_t::iterator it = this->dlg_main->dialog_arrays.begin();
            it != this->dlg_main->dialog_arrays.end();
            it++) {
            for (dialog_array::dialog_controls_t::iterator jt = (*it)->dialog_controls.begin();
                jt != (*it)->dialog_controls.end();
                jt++) {
                (*jt)->set_display_name(false, false);
            }
        }
        ShowWindow(SW_SHOW);
        BringWindowToTop();
        break;
    }

    return 0;
}

LRESULT window::OnSystemTrayIcon(UINT, WPARAM wParam, LPARAM lParam, BOOL & bHandled) {

    ATLASSERT(wParam == 1);
    switch (lParam) {
    case WM_LBUTTONUP:
        ShowOrHideWindow();
        break;
    case WM_RBUTTONUP:
        SetForegroundWindow(m_hWnd);

        CPoint pos;
        ATLVERIFY(GetCursorPos(&pos));

        CMenu menu;
        menu.LoadMenuW(IDR_TRAYMENU);
        CMenuHandle popupMenu = menu.GetSubMenu(0);

        popupMenu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON, pos.x, pos.y, this->m_hWnd);
        break;
    }
    return 0;
}

LRESULT window::OnFileRefreshlist(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (!this->dlg_main_b) {
        this->form_view->refresh_list();
    }
    return 0;
}

LRESULT window::OnAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    this->MessageBoxW(
        L"Audio Router version 0.10.2.\n" \
        L"\nIf you come across any bugs(especially relating to routing or duplicating), " \
        L"or just have an idea for a new feature, " \
        L"please send a PM to the developer on reddit: reddit.com/user/audiorouterdev/",
        L"About", MB_ICONINFORMATION);
    return 0;
}

LRESULT window::OnFileSwitchview(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    RECT rc;
    this->GetClientRect(&rc);

    if (this->dlg_main_b) {
        this->dlg_main->DestroyWindow();
        delete this->dlg_main;

        this->m_hWndClient = this->form_view->Create(*this);
        //this->form_view->ShowWindow(SW_SHOW);
        this->form_view->SetWindowPos(NULL, &rc, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    else {
        //TODO: by wolfreak99: Should form_view be deleted as well?
        this->form_view->DestroyWindow();

        this->dlg_main = new dialog_main(*this);
        this->m_hWndClient = this->dlg_main->Create(*this);
        //this->dlg_main->ShowWindow(SW_SHOW);
        this->dlg_main->SetWindowPos(NULL, &rc, SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    this->dlg_main_b = !this->dlg_main_b;

    return 0;
}

LRESULT window::OnFileExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    SendMessage(WM_QUIT);
    return 0;
}

LRESULT window::OnTrayMenuExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    SendMessage(WM_QUIT);
    return 0;
}

LRESULT window::OnTrayMenuShowHide(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    ShowOrHideWindow();
    return 0;
}

void window::ShowOrHideWindow()
{
    if (IsWindowOpen()) {
        SendMessage(WM_SYSCOMMAND, SC_CLOSE);
    }
    else {
        SendMessage(WM_SYSCOMMAND, SC_RESTORE);
    }
}

void window::ShowSystemTrayIcon()
{
    HideSystemTrayIcon();

    if (!m_NotifyIconData.cbSize) {
        m_NotifyIconData.cbSize = NOTIFYICONDATAA_V1_SIZE;
        m_NotifyIconData.hWnd = m_hWnd;
        m_NotifyIconData.uID = 1;
        m_NotifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_NotifyIconData.uCallbackMessage = WM_SYSTEMTRAYICON;
        m_NotifyIconData.hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        ATL::CString sWindowText;
        GetWindowText(sWindowText);
        _tcscpy_s(m_NotifyIconData.szTip, sWindowText);
    }

    Shell_NotifyIcon(NIM_ADD, &m_NotifyIconData);
}

void window::HideSystemTrayIcon()
{
    if (m_NotifyIconData.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &m_NotifyIconData);
    }
}

bool window::IsWindowOpen()
{
    return this->IsWindowVisible() && !this->IsIconic();
}