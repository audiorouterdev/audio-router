#include "dialog_main.h"
#include "window.h"
#include "app_inject.h"
#include <audiopolicy.h>
#include <cassert>

dialog_main::dialog_main(window& parent) : parent(parent)
{
}

dialog_main::~dialog_main()
{
    this->clear_dialog_arrays();
}

void dialog_main::clear_dialog_arrays()
{
    for(auto it = this->dialog_arrays.begin();
        it != this->dialog_arrays.end();
        it++)
    {
        delete *it;
    }
    this->dialog_arrays.clear();
}

void dialog_main::refresh_dialog_arrays()
{
    this->clear_dialog_arrays();

    // set default array
    dialog_array* dlg_array = new dialog_array(*this);
    dlg_array->Create(this->m_hWnd);
    dlg_array->set_device(NULL, L"Default Audio Device");
    dlg_array->refresh_dialog_controls();

    this->dialog_arrays.push_back(dlg_array);

    // set actual arrays
    app_inject backend;
    backend.populate_devicelist();
    app_inject::devices_t devices;
    app_inject::get_devices(devices);
    for(size_t i = 0; i < backend.device_names.size(); i++)
    {
        dlg_array = new dialog_array(*this);
        dlg_array->Create(this->m_hWnd);
        dlg_array->set_device(devices[i], backend.device_names[i]);
        dlg_array->refresh_dialog_controls();

        this->dialog_arrays.push_back(dlg_array);
    }

    this->reposition_dialog_arrays();
}

void dialog_main::reposition_dialog_arrays()
{
    RECT rc = {spacing_y, spacing_x, 0, 0};
    LONG win_y = 0, win_x = -1;
    for(dialog_arrays_t::iterator it = this->dialog_arrays.begin();
        it != this->dialog_arrays.end();
        it++)
    {
        RECT rc3, rc4;
        (*it)->GetClientRect(&rc3);
        this->GetClientRect(&rc4);
        if((rc.left + rc3.right + spacing_x) > rc4.right && *it != this->dialog_arrays[0])
        {
            rc.left = spacing_x;
            rc.top += rc3.bottom + spacing_y;
        }
        win_y = rc.top + rc3.bottom + spacing_y;
        win_x = max(win_x, rc3.right + 2 * spacing_x);

        (*it)->SetWindowPos(
            NULL, rc.left, rc.top, 0, 0, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOSIZE);

        // --- workaround for groupbox not showing properly on top of dialog controls
        // in dialog array view ---
        // TODO: clipping occured because in child dialog ws_clipchildren was set in
        // dlgresize_init function
        (*it)->GetWindowRect(&rc4);
        this->ScreenToClient(&rc4);
        (*it)->ctrl_group.SetWindowPos(NULL, &rc4, /*SWP_SHOWWINDOW | */SWP_NOZORDER);
        (*it)->ctrl_group.SetWindowTextW(L"");//(*it)->device_name.c_str());

        /*rc4.left += (*it)->width;
        (*it)->ctrl_splitter.SetWindowPos(NULL, rc4.left,*/ 
        // ------
 
        RECT rc2;
        (*it)->GetClientRect(&rc2);
        rc.left += rc2.right + spacing_x;
    }

    // for ensuring the labels and controls are drawn correctly
    
    this->GetClientRect(&rc);
    //this->m_ptMinTrackSize.y = win_y;
    //this->DlgResize_UpdateLayout(rc.right, win_y);
    this->m_scrollHelper.SetDisplaySize(win_x, win_y);
    this->m_scrollHelper.ScrollToOrigin(true, true);
    //this->SetWindowPos(NULL, 0, 0, rc.right, win_y, SWP_NOZORDER);
    this->RedrawWindow();
}

dialog_array* dialog_main::find_control(DWORD pid, dialog_control** control, size_t index)
{
    size_t current_index = 0;
    for(dialog_arrays_t::iterator it = this->dialog_arrays.begin();
        it != this->dialog_arrays.end();
        it++)
    {
        *control = (*it)->find_control(pid);
        if(*control != NULL)
        {
            if(current_index == index)
                return *it;
            else
                current_index++;
        }
    }

    *control = NULL;
    return NULL;
}

LRESULT dialog_main::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    {
        RECT rc = {spacing_x, spacing_y, 0, 0};
        this->MapDialogRect(&rc);
        const_cast<int&>(spacing_x) = rc.left;
        const_cast<int&>(spacing_y) = rc.top;
    }

    this->refresh_dialog_arrays();
    this->m_scrollHelper.AttachWnd(this);

    //// enable tab support
    //LONG style = this->GetWindowLongW(GWL_EXSTYLE);
    //style |= WS_EX_CONTROLPARENT;
    //this->SetWindowLongW(GWL_EXSTYLE, style);
    /*CDialogMessageHook::InstallHook(*this);*/

    this->SetTimer(IDD_TIMER, TIMER_INTERVAL);

    return TRUE;
}

LRESULT dialog_main::OnCtrlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return (LRESULT)AtlGetStockBrush(WHITE_BRUSH);
}

LRESULT dialog_main::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->reposition_dialog_arrays();

    const UINT nType = wParam;
    const int cx = LOWORD(lParam), cy = HIWORD(lParam);
    this->m_scrollHelper.OnSize(nType, cx, cy);

    return 0;
}

LRESULT dialog_main::OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    const UINT nSBCode = LOWORD(wParam);
    const UINT nPos = HIWORD(wParam);
    if(lParam != NULL)
        this->m_scrollHelper.OnHScroll(nSBCode, nPos, &CScrollBar((HWND)lParam));
    else
        this->m_scrollHelper.OnHScroll(nSBCode, nPos, NULL);

    return 0;
}

LRESULT dialog_main::OnVScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    const UINT nSBCode = LOWORD(wParam);
    const UINT nPos = HIWORD(wParam);
    if(lParam != NULL)
        this->m_scrollHelper.OnVScroll(nSBCode, nPos, &CScrollBar((HWND)lParam));
    else
        this->m_scrollHelper.OnVScroll(nSBCode, nPos, NULL);

    return 0;
}

LRESULT dialog_main::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    const UINT nFlags = LOWORD(wParam);
    const short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    const CPoint point(LOWORD(lParam), HIWORD(lParam));

    this->m_scrollHelper.OnMouseWheel(nFlags, zDelta, point);

    return 0;
}

LRESULT dialog_main::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /*CDialogMessageHook::UninstallHook(*this);*/
    return 0;
}

LRESULT dialog_main::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(this->parent.IsIconic())
        return 0;

    for(dialog_arrays_t::iterator it = this->dialog_arrays.begin();
        it != this->dialog_arrays.end();
        it++)
    {
        (*it)->update_controls();
    }

    return 0;
}

LRESULT dialog_main::OnBnEnter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
    ::SendMessage(::GetParent(hWndCtl), WM_COMMAND, IDC_BUTTON1 | BN_CLICKED, (LPARAM)hWndCtl);
    return 0;
}