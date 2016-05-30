#pragma once

#include "wtl.h"
#include "ScrollHelper.h"
#include "DialogMessageHook.h"
#include "dialog_control.h"
#include "dialog_array.h"
#include <vector>

class window;

#define IDD_TIMER 1
#define TIMER_INTERVAL 1000

// TODO: register listener for device events in dialog_main

class dialog_main : public CDialogImpl<dialog_main>
{
public:
    typedef std::vector<dialog_array*> dialog_arrays_t;
private:
    CButton ctrl_groupbox;
    CScrollHelper m_scrollHelper;

    void clear_dialog_arrays();
public:
    const int spacing_x = 7, spacing_y = 7;
    dialog_arrays_t dialog_arrays;
    window& parent;

    enum {IDD = IDD_MAINDLG};

    dialog_main(window&);
    ~dialog_main();

    void refresh_dialog_arrays();
    void reposition_dialog_arrays();
    dialog_array* find_control(DWORD pid, dialog_control**, size_t index = 0);

    BEGIN_MSG_MAP(dialog_main)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtrlColor)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
        MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        // dialog message procedure sends this when enter is pressed
        COMMAND_ID_HANDLER(IDC_BUTTON1, OnBnEnter)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCtrlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnHScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnVScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnMouseWheel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnBnEnter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};