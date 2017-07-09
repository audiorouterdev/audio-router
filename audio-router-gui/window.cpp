#include "window.h"

#ifndef DISABLE_TELEMETRY
telemetry* telemetry_m = NULL;
#endif
HMENU trayIconMenu;

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
    STray.RemoveIcon();
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

    bIsVisible = true;
    STray.hWnd = this->m_hWnd;
    STray.SetTipText("Audio Router");
    STray.AddIcon();
    return 0;
}

LRESULT window::OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

    if (wParam == SC_MINIMIZE) {
        for (dialog_main::dialog_arrays_t::iterator it = this->dlg_main->dialog_arrays.begin();
             it != this->dlg_main->dialog_arrays.end();
             it++)
        {
            for (dialog_array::dialog_controls_t::iterator jt = (*it)->dialog_controls.begin();
                 jt != (*it)->dialog_controls.end();
                 jt++)
            {
                (*jt)->set_display_name(false, true);
            }

        }

        this->ShowWindow(SW_HIDE);
        bIsVisible = false;
        return 0;

    } else if (wParam == SC_RESTORE) {
        for (dialog_main::dialog_arrays_t::iterator it = this->dlg_main->dialog_arrays.begin();
             it != this->dlg_main->dialog_arrays.end();
             it++)
        {
            for (dialog_array::dialog_controls_t::iterator jt = (*it)->dialog_controls.begin();
                 jt != (*it)->dialog_controls.end();
                 jt++)
            {
                (*jt)->set_display_name(false, false);
            }
        }
    } else if (wParam == SC_CLOSE) {
        this->ShowWindow(SW_HIDE);
        bIsVisible = false;
        return 0;
    }


    bHandled = FALSE;
    return 0;
}

LRESULT window::OnTrayNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
{
    if (trayIconMenu != NULL) {
        DestroyMenu(trayIconMenu);
        trayIconMenu = NULL;
    }

    switch (LOWORD(lParam))
    {
        case WM_LBUTTONUP:
            if (bIsVisible) {
                this->ShowWindow(SW_HIDE);
                bIsVisible = false;
            }
            else {
                this->ShowWindow(SW_SHOW);
                this->BringWindowToTop();
                bIsVisible = true;
            }
            break;
        case WM_RBUTTONUP:
            trayIconMenu = CreatePopupMenu();
            
            UINT menuFlags = MF_BYPOSITION | MF_STRING;
            InsertMenuW(trayIconMenu, -1, menuFlags, ID_TRAYMENU_SHOWHIDE, _T("Show/hide"));
            InsertMenuW(trayIconMenu, -1, menuFlags, ID_FILE_EXIT, _T("Exit"));

            POINT lpClickPoint;
            GetCursorPos(&lpClickPoint);
            
            int nReserved = 0;
            
            TrackPopupMenu(trayIconMenu,
                TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
                lpClickPoint.x, lpClickPoint.y,
                nReserved, this->m_hWnd, NULL
            );
            break;
    }
    return 0;
}

LRESULT window::OnFileRefreshlist(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if (!this->dlg_main_b)
    {
        this->form_view->refresh_list();
    }
    return 0;
}

LRESULT window::OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->MessageBoxW(
        L"Audio Router version 0.10.2.\n" \
        L"\nIf you come across any bugs(especially relating to routing or duplicating), " \
        L"or just have an idea for a new feature, " \
        L"please send a PM to the developer on reddit: reddit.com/user/audiorouterdev/",
        L"About", MB_ICONINFORMATION);
    return 0;
}

LRESULT window::OnFileSwitchview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    RECT rc;
    this->GetClientRect(&rc);

    if (this->dlg_main_b)
    {
        this->dlg_main->DestroyWindow();
        delete this->dlg_main;

        this->m_hWndClient = this->form_view->Create(*this);
        //this->form_view->ShowWindow(SW_SHOW);
        this->form_view->SetWindowPos(NULL, &rc, SWP_NOZORDER | SWP_SHOWWINDOW);
    } else
    {
        this->form_view->DestroyWindow();

        this->dlg_main = new dialog_main(*this);
        this->m_hWndClient = this->dlg_main->Create(*this);
        //this->dlg_main->ShowWindow(SW_SHOW);
        this->dlg_main->SetWindowPos(NULL, &rc, SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    this->dlg_main_b = !this->dlg_main_b;

    return 0;
}

LRESULT window::OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return HTCLOSE;
}

LRESULT window::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    PostQuitMessage(0);
    return 0;
}

LRESULT window::OnTrayMenuExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if (trayIconMenu != NULL) {
        DestroyMenu(trayIconMenu);
        trayIconMenu = NULL;
    }

    PostQuitMessage(0);
    return 0;
}

LRESULT window::OnTrayMenuShowHide(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if (trayIconMenu != NULL) {
        DestroyMenu(trayIconMenu);
        trayIconMenu = NULL;
    }

    if (bIsVisible) {
        this->ShowWindow(SW_HIDE);
        bIsVisible = false;
    }
    else {
        this->ShowWindow(SW_SHOW);
        this->BringWindowToTop();
        bIsVisible = true;
    }
    return 0;
}
