#pragma once

#include "wtl.h"
#include "app_list.h"
#include "app_inject.h"
#ifndef DISABLE_TELEMETRY
#include "telemetry.h"
#endif
#include <vector>
#include <string>
#include <map>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

class window;

class input_dialog : public CDialogImpl<input_dialog>
{
private:
    CComboBox combobox_m;
    int mode;
public:
    enum {IDD = IDD_ROUTEDIALOG};
    int selected_index;
    bool forced;

    explicit input_dialog(int mode = 0);

    BEGIN_MSG_MAP(input_dialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnBnClickedOk)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnBnClickedCancel)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};

class formview : 
    public CDialogImpl<formview>, 
    public CDialogResize<formview>
{
public:
    typedef std::map<DWORD, std::pair<std::wstring, bool>> routed_processes_t;
private:
    CListViewCtrl listview_m;
    window& parent;
    app_list compatible_apps;
    routed_processes_t routed_processes;
    //telemetry telemetry_m;

    void add_item(const std::wstring& name, const std::wstring& routed_to);
    void open_dialog();
public:
    enum {IDD = IDD_FORMVIEW};

    formview(window&);
    ~formview();

    void refresh_list();

    BEGIN_MSG_MAP(formview)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        CHAIN_MSG_MAP(CDialogResize<formview>)
        NOTIFY_HANDLER(IDC_LIST1, NM_DBLCLK, OnDoubleClick)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(formview)
        DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDoubleClick(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/);
};