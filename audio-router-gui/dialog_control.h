#pragma once

#include "wtl.h"
#include "DialogMessageHook.h"
#include "dialog_array.h" // for friend function
#include <endpointvolume.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <memory>

class dialog_array;
struct IAudioSessionControl2;
struct ISimpleAudioVolume;
struct IAudioSessionEvents;
class dialog_control;

class custom_trackbar_ctrl : 
    public CWindowImpl<custom_trackbar_ctrl, CTrackBarCtrl>,
    public CCustomDraw<custom_trackbar_ctrl>
{
public:
// http://www.codeproject.com/Articles/853/Custom-Drawn-Controls-using-WTL
#if (_WTL_VER >= 0x0700)
   BOOL m_bHandledCD;
 
   BOOL IsMsgHandled() const
   {
      return m_bHandledCD;
   }
 
   void SetMsgHandled(BOOL bHandled)
   {
      m_bHandledCD = bHandled;
   }
#endif //(_WTL_VER >= 0x0700)
public:
    dialog_control& ctrl;
    custom_trackbar_ctrl(dialog_control& ctrl) : ctrl(ctrl) {}

    BEGIN_MSG_MAP(custom_trackbar_ctrl)
        CHAIN_MSG_MAP(CCustomDraw<custom_trackbar_ctrl>)
    END_MSG_MAP()

    HWND dlg_parent;

    DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
    DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw);
    DWORD OnItemPostPaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
};

class dialog_control : public CDialogImpl<dialog_control>, public COwnerDraw<dialog_control>
{
    friend dialog_control* dialog_array::create_control(DWORD, IAudioSessionControl2*, bool);
public:
    typedef std::vector<IAudioSessionControl2*> audio_sessions_t;
    typedef std::vector<ISimpleAudioVolume*> audio_volumes_t;
    typedef std::vector<IAudioSessionEvents*> audio_events_t;
    typedef std::vector<CComPtr<IAudioMeterInformation>> audio_meters_t;
    enum routing_state_t {ROUTING, NO_STATE};
private:
    class session_events;
private:
    HANDLE handle_process;
    HICON icon;
    CButton ctrl_button, ctrl_group; 
    CStatic ctrl_image, ctrl_static;
    custom_trackbar_ctrl ctrl_slider;
    audio_sessions_t audio_sessions;
    audio_volumes_t audio_volumes;
    // audio events have 1:1 mapping to audio sessions
    audio_events_t audio_events;
    // audio meters have 1:1 mapping to audio sessions
    audio_meters_t audio_meters;
    std::wstring display_name;
    bool muted, duplicating, managed;
    float peak_meter_value, peak_meter_velocity;

    dialog_control(dialog_array&, DWORD pid);
    void do_route(bool duplication);
    void update_attributes();
public:
    // TODO: contorller embedded in this class
    std::unique_ptr<Gdiplus::Bitmap> img;

    // as pixels
    const int width, height;
    const int spacing_x = 7, spacing_y = 7;
    const DWORD pid;
    bool x86;
    routing_state_t routing_state;
    dialog_array& parent;

    enum {IDD = IDD_CONTROLDLG};

    ~dialog_control();

    void delete_audio_sessions();
    bool add_audio_session(IAudioSessionControl2*);
    const audio_sessions_t& get_audio_sessions() const {return this->audio_sessions;}
    void set_volume(int level, bool set = true);
    void set_mute(bool);
    void set_display_name(bool set_icon = true, bool show_process_name = false);
    bool is_muted() const {return this->muted;}
    bool is_managed() const {return this->managed;}

    BEGIN_MSG_MAP(dialog_control)
        CHAIN_MSG_MAP(COwnerDraw<dialog_control>)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        //MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtrlColor)
        COMMAND_HANDLER(IDC_BUTTON1, BN_CLICKED, OnBnClickedButton)
        COMMAND_ID_HANDLER(ID_POPUP_ROUTE, OnPopupRoute)
        NOTIFY_HANDLER(IDC_SLIDER1, TRBN_THUMBPOSCHANGING, OnVolumeChange)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_ID_HANDLER(ID_POPUP_MUTE, OnPopupMute)
        COMMAND_ID_HANDLER(ID_POPUP_DUPLICATE, OnPopupDuplicate)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        CHAIN_MSG_MAP_MEMBER(ctrl_slider)
    #ifdef ENABLE_BOOTSTRAP
        COMMAND_ID_HANDLER(ID_SAVEDROUTINGS_SAVEROUTINGFORTHISAPP, OnPopUpSave)
        COMMAND_ID_HANDLER(ID_SAVEDROUTINGS_DELETEALLROUTINGSFORTHISAPP, OnPopUpDelete)
    #endif
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCtrlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnBnClickedButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnPopupRoute(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnVolumeChange(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnPopupMute(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnPopupDuplicate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    void DrawItem(LPDRAWITEMSTRUCT);
    LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnPopUpSaveRouting(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef ENABLE_BOOTSTRAP
    LRESULT OnPopUpSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
    LRESULT OnPopUpDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
};