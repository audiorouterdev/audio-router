#pragma once

#include "wtl.h"
#include "app_inject.h"
#include "DialogMessageHook.h"
#include <string>
#include <list>

class dialog_main;
class dialog_control;
struct IAudioSessionControl;
struct IAudioSessionControl2;
struct IAudioSessionManager2;
struct IAudioEndpointVolume;

#define DLG_CONTROL_WIDTH 83 // real dialog width(including border size(=1))
#define DLG_CONTROL_HEIGHT 174 // real dialog height
#define WM_SESSION_CREATED (WM_APP + 1)

UINT ExtractDeviceIcons(LPWSTR iconPath,HICON *iconLarge,HICON *iconSmall);

class dialog_array : public CDialogImpl<dialog_array>, public COwnerDraw<dialog_array>
{
public:
    typedef std::list<dialog_control*> dialog_controls_t;
    typedef app_inject::devices_t::value_type device_t;
    typedef IAudioEndpointVolume* audio_volume_t;
private:
    class session_notifications;
private:
    CButton ctrl_button;
    CStatic ctrl_image, ctrl_static;
    //cctrl_static ctrl_static;

    HICON icon;
    CTrackBarCtrl ctrl_slider;
    session_notifications* session_notif;
    audio_volume_t audio_volume;
    device_t device;
    device_t default_device;
    IAudioSessionManager2* session_manager;

    void choose_array_and_create_control(IAudioSessionControl*);
    void clear_dialog_controls();
    // returns 0 in case of error
    static DWORD get_audio_session_control(IAudioSessionControl*, IAudioSessionControl2**);
public:
    const int spacing_x = 7, spacing_y = 7;
    const int width = DLG_CONTROL_WIDTH, height = DLG_CONTROL_HEIGHT;
    CButton ctrl_group;
    CStatic ctrl_splitter;
    std::wstring device_name;
    dialog_main& parent;
    dialog_controls_t dialog_controls;

    enum {IDD = IDD_CONTROLDLG};

    dialog_array(dialog_main&);
    ~dialog_array();

    device_t get_device() const {return this->device;}
    void set_device(device_t device, const std::wstring& device_name);
    // TODO: rename to initialize
    void refresh_dialog_controls();
    void reposition_dialog_controls();
    dialog_control* create_control(DWORD, IAudioSessionControl2*, bool reposition = true);
    bool delete_control(DWORD, bool reposition = true);
    dialog_controls_t::iterator find_control_it(DWORD pid);
    dialog_control* find_control(DWORD pid);
    bool take_control(DWORD, dialog_array*);
    void update_controls();
    void set_volume(int level, bool set = true);

    BEGIN_MSG_MAP(dialog_array)
        CHAIN_MSG_MAP(COwnerDraw<dialog_array>)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        //MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_SESSION_CREATED, OnSessionCreated)
        NOTIFY_HANDLER(IDC_SLIDER1, TRBN_THUMBPOSCHANGING, OnVolumeChange)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_HANDLER(IDC_BUTTON1, BN_CLICKED, OnBnClickedButton1)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnSessionCreated(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnVolumeChange(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnBnClickedButton1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    void DrawItem(LPDRAWITEMSTRUCT);
};