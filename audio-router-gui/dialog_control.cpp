#include "dialog_control.h"
#include "dialog_main.h"
#include "dialog_array.h"
#include "app_list.h"
#include "formview.h"
#include "window.h"
#include <shellapi.h>
#include <audiopolicy.h>
#include <Audioclient.h>
#include <Psapi.h>
#include <cassert>

#define IDD_TIMER_CONTROL_DLG 2
#define TIMER_INTERVAL_CONTROL_DLG 10

extern telemetry* telemetry_m;

DWORD custom_trackbar_ctrl::OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
{
    return CDRF_NOTIFYITEMDRAW;
}

DWORD custom_trackbar_ctrl::OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
{
    return CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
}

DWORD custom_trackbar_ctrl::OnItemPostPaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
{
    if(lpNMCustomDraw->dwItemSpec == TBCD_CHANNEL)
    {
        CDCHandle hdc = lpNMCustomDraw->hdc;
        using namespace Gdiplus;
        const INT offset = 1;
        Graphics g(lpNMCustomDraw->hdc);
        g.DrawImage(this->ctrl.img.get(), 
            lpNMCustomDraw->rc.left + offset, lpNMCustomDraw->rc.top + offset,
            this->ctrl.img->GetWidth() - offset * 2, this->ctrl.img->GetHeight() - offset * 2);


        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

// http://stackoverflow.com/questions/1888863/how-to-get-main-window-handle-from-process-id
struct handle_data {
    unsigned long process_id;
    HWND best_handle;
};

BOOL is_main_window(HWND handle)
{   
    // http://stackoverflow.com/questions/2262726/determining-if-a-window-has-a-taskbar-button
    if(!IsWindowVisible(handle))
        return FALSE;

    LONG_PTR Style = GetWindowLongPtr(handle, GWL_STYLE);
    LONG_PTR ExStyle = GetWindowLongPtr(handle, GWL_EXSTYLE);
    if(ExStyle & WS_EX_APPWINDOW)
        return TRUE;
    if(ExStyle & WS_EX_TOOLWINDOW)
        return FALSE;
    if(Style & WS_CHILD)
        return FALSE;

    int i = 0;
    if(Style & WS_OVERLAPPED)
        i++;
    if(Style & WS_POPUP)
    {
        i--;
        if(GetParent(handle) != NULL)
            i--;
    }
    if(ExStyle & WS_EX_OVERLAPPEDWINDOW)
        i++;
    if(ExStyle & WS_EX_CLIENTEDGE)
        i--;
    if(ExStyle & WS_EX_DLGMODALFRAME)
        i--;
    return (i >= 0);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    handle_data& data = *(handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !is_main_window(handle)) {
        return TRUE;
    }
    data.best_handle = handle;
    return FALSE;   
}

HWND find_main_window(unsigned long process_id)
{
    handle_data data;
    data.process_id = process_id;
    data.best_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.best_handle;
}

class dialog_control::session_events : public IAudioSessionEvents
{
private:
    LONG _cRef;
    dialog_control& parent;
    IAudioSessionControl2* session;
public:
    session_events(dialog_control& parent, IAudioSessionControl2* session) : 
        _cRef(1), parent(parent), session(session) {}

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&this->_cRef);
    }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&this->_cRef);
        if(ulRef == 0)
        {
            delete this;
        }
        return ulRef;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID  riid, VOID** ppvInterface)
    {
        if(riid == IID_IUnknown)
        {
            this->AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if(riid == __uuidof(IAudioSessionEvents))
        {
            this->AddRef();
            *ppvInterface = (IAudioSessionEvents*)this;
        }
        else
        {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext)
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext)
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(
        float NewVolume, BOOL NewMute,LPCGUID EventContext)
    {
        // TODO: change the controller's slider position and
        // possibly change the audio of all sessions in this controller
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(
        DWORD ChannelCount, float NewChannelVolumeArray[], 
        DWORD ChangedChannel, LPCGUID EventContext)
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(
        LPCGUID NewGroupingParam, LPCGUID EventContext)
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState)
    {
        // wasapi does not fire events properly
        //// TODO: remove session from controller if state has changed to expired
        //if(NewState == AudioSessionStateExpired)
        //{
        //    OutputDebugStringW(L"session expired\n");
        //    for(audio_sessions_t::iterator it = this->parent.audio_sessions.begin();
        //        it != this->parent.audio_sessions.end();
        //        it++)
        //    {
        //        if(*it == this->session)
        //        {
        //            dialog_control& parent = this->parent;

        //            ULONG refc = this->Release();
        //            assert(refc == 1);
        //            if(refc == 0)
        //            {
        //                parent.audio_sessions.erase(it);
        //                (*it)->Release();
        //                // TODO: reposition should reposition its parent too
        //                parent.parent.reposition_dialog_controls();
        //                parent.parent.parent.reposition_dialog_arrays();
        //                return S_OK;
        //            }

        //            if(this->session->UnregisterAudioSessionNotification(this) != S_OK)
        //                this->Release();
        //            parent.audio_sessions.erase(it);
        //            (*it)->Release();
        //            parent.parent.reposition_dialog_controls();
        //            parent.parent.parent.reposition_dialog_arrays();

        //            // this is destroyed at this point
        //            return S_OK;
        //        }
        //    }

        //    // there should be one session in the controller for this event
        //    assert(false);
        //}

        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason)
    {
        // TODO: remove session
        return S_OK;
    }
};

HWND g_HWND=NULL;
BOOL CALLBACK EnumWindowsProcMy(HWND hwnd,LPARAM lParam)
{
    DWORD lpdwProcessId;
    GetWindowThreadProcessId(hwnd,&lpdwProcessId);
    if(lpdwProcessId==lParam)
    {
        g_HWND=hwnd;
        return FALSE;
    }
    return TRUE;
}

LRESULT CALLBACK MenuButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // http://poiru.net/2012/06/17/win32-menu-button
    switch (uMsg)
    {
    case WM_PAINT:
        {
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            RECT buttonRect;
            GetClientRect(hWnd, &buttonRect); // 33 35
            int arrowX = buttonRect.right / 4 + 1;
            int arroyY = buttonRect.bottom / 4;
            RECT arrowRect = { arrowX, arroyY, arrowX * 3, arroyY * 3 };      

            // Draw arrow on top of the button
            HDC dc = GetDC(hWnd);
            const UINT DFCS_MENUARROWDOWN = 0x0010; // Undocumented 
            DWORD drawFlags = DFCS_TRANSPARENT | DFCS_MENUARROWDOWN | (IsWindowEnabled(hWnd) ? 0 : DFCS_INACTIVE);
            DrawFrameControl(dc, &arrowRect, DFC_MENU, drawFlags);
            ReleaseDC(hWnd, dc);

            return result;
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, MenuButtonProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

dialog_control::dialog_control(dialog_array& parent, DWORD pid) : 
    pid(pid), width(DLG_CONTROL_WIDTH), height(DLG_CONTROL_HEIGHT), 
    routing_state(NO_STATE), x86(true), parent(parent), muted(false), duplicating(false),
    icon(NULL), ctrl_slider(*this), peak_meter_value(0.0f), peak_meter_velocity(0.0f),
    managed(false)
{
    // acquire handle so the pid won't be invalidated until this is destroyed
    this->handle_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->pid);
    assert(this->handle_process);
}

dialog_control::~dialog_control()
{
    if(this->icon)
        DestroyIcon(this->icon);
    this->delete_audio_sessions();
    CloseHandle(this->handle_process);
}

void dialog_control::update_attributes()
{
    std::wstring text = this->display_name;
    if(this->routing_state != NO_STATE)
        if(this->duplicating)
            text = L"(Duplication Pending)\n" + text;
        else
            text = L"(Routing Pending)\n" + text;
    if(this->managed)
        text = L"(Saved Routing)\n" + text;

    ATL::CString str;
    this->ctrl_static.GetWindowTextW(str);

    if(!text.empty() && text.compare(str) != 0)
        this->ctrl_static.SetWindowTextW(text.c_str());
}

void dialog_control::set_display_name(bool set_icon, bool show_process_name)
{
    std::wstring text;
    CWindow wnd;

    // http://articles.forensicfocus.com/2015/08/06/standard-processes-in-windows-10/
    app_list::app_info app;
    app.id = this->pid;
    if(app_list::get_app_info(app))
    {
        this->x86 = app.x86;
        text = app.name;
        if(text == L"taskhost.exe" || text == L"taskhostex.exe" || text == L"taskhostw.exe")
        {
            if(!show_process_name)
            {
                text = L"System Sounds";
                if(!this->icon)
                {
                    HICON smallicon = NULL, large = NULL;
                    LPWSTR mmres = StrDupW(L"%windir%\\system32\\mmres.dll,-3004");
                    if(ExtractDeviceIcons(mmres, &large, &smallicon))
                    {
                        if(large)
                        {
                            if(smallicon)
                                DestroyIcon(smallicon);
                            this->icon = large;
                        }
                        else if(smallicon)
                            this->icon = smallicon;
                        else
                            this->icon = NULL;
                    }
                    LocalFree(mmres);
                }
                goto success;
            }
            else
                text += L" (System Sounds)";
        }
        if(show_process_name)
            goto success;

        HWND hwnd = find_main_window(this->pid);
        if(hwnd)
        {
            wnd = hwnd;
            ATL::CString str;
            wnd.GetWindowTextW(str);
            text = str;
        }
        goto success;
    }
    text = L"Display name not available";

success:
    if(this->display_name != text)
    {
        if(!text.empty())
            this->display_name = text;
        this->update_attributes();
    }

    TCHAR filename[MAX_PATH];
    if(!this->icon && GetModuleFileNameEx(this->handle_process, NULL, filename, MAX_PATH))
    {
        WORD icon_index = 0;
        this->icon = ExtractAssociatedIcon(GetModuleHandle(NULL), filename, &icon_index);
    }
    if(set_icon && this->icon)
        this->ctrl_image.SetIcon(this->icon);
}

bool dialog_control::add_audio_session(IAudioSessionControl2* session)
{
    if(!session)
        return false;

    this->audio_sessions.push_back(session);

    // add volume controller for session
    ISimpleAudioVolume* audio_volume = NULL;
    if(session->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audio_volume) == S_OK)
    {
        // update state of this session to correspond to older states
        float level = 1.f;
        BOOL mute = FALSE;
        if(!this->audio_volumes.empty())
        {
            audio_volumes_t::reference back = this->audio_volumes.back();
            back->GetMasterVolume(&level);
            back->GetMute(&mute);
        }
        else
        {
            audio_volume->GetMasterVolume(&level);
            audio_volume->GetMute(&mute);
        }
        this->audio_volumes.push_back(audio_volume);
        this->set_volume((int)(level * 100.f));
        this->set_mute(mute);
    }
    else
        SAFE_RELEASE(audio_volume);

    // add session event listener for session
    session_events* session_event;
    if(session->RegisterAudioSessionNotification(
        session_event = new session_events(*this, session)) == S_OK)
    {
        this->audio_events.push_back(session_event);
    }
    else
    {
        this->audio_events.push_back(NULL);
        session_event->Release();
    }

    // add peak meter for session
    CComPtr<IAudioMeterInformation> audio_meter;
    session->QueryInterface(__uuidof(IAudioMeterInformation), (void**)&audio_meter);
    this->audio_meters.push_back(audio_meter);

    return true;
}

void dialog_control::delete_audio_sessions()
{
    assert(this->audio_events.size() == this->audio_sessions.size());

    // clear volume controllers
    for(auto it = this->audio_volumes.begin();
        it != this->audio_volumes.end();
        it++)
        (*it)->Release();
    this->audio_volumes.clear();

    // clear session event listeners
    for(size_t i = 0; i < this->audio_events.size(); i++)
    {
        if(this->audio_events[i] != NULL)
        {
            ULONG c = this->audio_events[i]->Release();
            assert(c > 0);
            if(this->audio_sessions[i]->UnregisterAudioSessionNotification(this->audio_events[i]) !=
                S_OK)
            {
                this->audio_events[i]->Release();
            }
        }
    }
    this->audio_events.clear();

    // clear meters
    this->audio_meters.clear();

    // clear sessions
    for(auto it = this->audio_sessions.begin();
        it != this->audio_sessions.end();
        it++)
        ULONG refc = (*it)->Release();
    this->audio_sessions.clear();
}

void dialog_control::set_volume(int level, bool set)
{
    // TODO: set mute state when creating controls and arrays;
    // decide how to indicate/handle mixed state sessions

    if(level > 100)
        level = 100;
    else if(level < 0)
        level = 0;

    this->ctrl_slider.SetPos(100 - level);
    if(set)
    {
        for(audio_volumes_t::iterator it = this->audio_volumes.begin();
            it != this->audio_volumes.end();
            it++)
        {
            (*it)->SetMasterVolume(((float)level) / 100.f, NULL);
        }
    }
}

void dialog_control::set_mute(bool mute)
{
    this->muted = mute;

    for(audio_volumes_t::iterator it = this->audio_volumes.begin();
        it != this->audio_volumes.end();
        it++)
    {
        if(this->muted)
        {
            if((*it)->SetMute(TRUE, NULL) == S_OK)
            {
                //this->GetDlgItem(ID_POPUP_MUTE).SetWindowTextW(L"Mute");
                this->ctrl_slider.EnableWindow(FALSE);
            }
        }
        else
        {
            if((*it)->SetMute(FALSE, NULL) == S_OK)
            {
                //this->GetDlgItem(ID_POPUP_MUTE).SetWindowTextW(L"Unmute");
                this->ctrl_slider.EnableWindow(TRUE);
            }
        }
    }
}

void dialog_control::do_route(bool duplication)
{
    // TODO: decide if update the pid of sessions here

    input_dialog input_dlg(duplication);
    input_dlg.DoModal(*this);
    if(IsBadReadPtr(this, sizeof(dialog_control)))
        return;
    const int sel_index = input_dlg.selected_index;
    app_inject::flush_t flush = input_dlg.forced ? app_inject::HARD : app_inject::SOFT;

    if(duplication && sel_index == 0)
    {
        this->MessageBoxW(L"You cannot duplicate to default device!", 
            L"Notice", MB_ICONINFORMATION);
        return;
    }

    if(sel_index >= 0)
    {
        if(telemetry_m)
            telemetry_m->update_on_routing();

        app_inject injector;
        try
        {
            injector.populate_devicelist();
            injector.inject(this->pid, this->x86, sel_index, flush, duplication);
        }
        catch(std::wstring err)
        {
            err += L"Router functionality not available.";
            this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
            return;
        }

        this->routing_state = (sel_index != 0 ? ROUTING : NO_STATE);
        if(this->routing_state == NO_STATE)
            this->parent.PostMessageW(WM_SESSION_CREATED, NULL, (LPARAM)this);

        this->duplicating = duplication;
        this->update_attributes();
    }
}

LRESULT dialog_control::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    {
        RECT rc = {spacing_x, spacing_y, width, height};
        this->MapDialogRect(&rc);
        const_cast<int&>(width) = rc.right;
        const_cast<int&>(height) = rc.bottom;
        const_cast<int&>(spacing_x) = rc.left;
        const_cast<int&>(spacing_y) = rc.top;
    }

    // retrieve items
    this->ctrl_button = this->GetDlgItem(IDC_BUTTON1);
    this->ctrl_group = this->GetDlgItem(IDC_GROUP1);
    this->ctrl_image = this->GetDlgItem(IDC_IMAGE);
    this->ctrl_slider.SubclassWindow(this->GetDlgItem(IDC_SLIDER1));
    this->ctrl_static = this->GetDlgItem(IDC_STATIC1);
    this->ctrl_static.SetWindowLongW(
        GWL_STYLE, (this->ctrl_static.GetWindowLongW(GWL_STYLE) | SS_OWNERDRAW));
    this->ctrl_group.ShowWindow(SW_HIDE);
    SetWindowSubclass(this->ctrl_button, MenuButtonProc, NULL, NULL);
    this->ctrl_slider.dlg_parent = *this;
    //this->ctrl_slider.SubclassWindow(ctrl_slider);

    // remove border
    LONG style = this->GetWindowLongW(GWL_STYLE);
    style &= ~WS_BORDER;
    this->SetWindowLongW(GWL_STYLE, style);

    // set text
    this->display_name = L"Display name not available";
    this->ctrl_static.SetWindowTextW(this->display_name.c_str());

    // set slider control
    this->ctrl_slider.SetRange(0, 100);
    this->ctrl_slider.SetPos(0);
    style = this->ctrl_slider.GetWindowLongW(GWL_STYLE);
    style |= TBS_NOTIFYBEFOREMOVE;
    this->ctrl_slider.SetWindowLongW(GWL_STYLE, style);

    // promote the controls in this dialog to be child controls
    // of the parent dialog
    style = this->GetWindowLongW(GWL_EXSTYLE);
    style |= WS_EX_CONTROLPARENT;
    this->SetWindowLongW(GWL_EXSTYLE, style);
    /*CDialogMessageHook::InstallHook(*this);*/

    // add timer
    this->SetTimer(IDD_TIMER_CONTROL_DLG, TIMER_INTERVAL_CONTROL_DLG);

    // create bitmap of channel's size
    RECT rc;
    this->ctrl_slider.GetChannelRect(&rc);
    using namespace Gdiplus;
    this->img.reset(new Bitmap(rc.bottom - rc.top, rc.right - rc.left));

    // check if the application is implicitly routed, aka managed
    bootstrapper& bootstrap = *this->parent.parent.parent.bootstrap;
    this->managed = bootstrap.is_saved_routing(this->pid, this->parent.get_device());

    return 0;
}

LRESULT dialog_control::OnCtrlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return (LRESULT)AtlGetStockBrush(WHITE_BRUSH);
}

LRESULT dialog_control::OnBnClickedButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    RECT rc;
    this->ctrl_button.GetWindowRect(&rc);

    CMenu menu;
    menu.LoadMenuW(IDR_LISTVIEWMENU);
    CMenuHandle listviewmenu = menu.GetSubMenu(0);
    listviewmenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rc.left, rc.bottom, this->m_hWnd);

    return 0;
}


LRESULT dialog_control::OnPopupRoute(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->do_route(false);
    return 0;
}


LRESULT dialog_control::OnVolumeChange(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/)
{
    // This feature requires Windows Vista or greater.
    // The symbol _WIN32_WINNT must be >= 0x0600.
    NMTRBTHUMBPOSCHANGING *pNMTPC = reinterpret_cast<NMTRBTHUMBPOSCHANGING *>(pNMHDR);
    
    this->set_volume(100 - (int)pNMTPC->dwPos);

    return 0;
}

LRESULT dialog_control::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /*CDialogMessageHook::UninstallHook(*this);*/
    return 0;
}

LRESULT dialog_control::OnPopupMute(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->muted = !this->muted;
    this->set_mute(this->muted);

    return 0;
}


LRESULT dialog_control::OnPopupDuplicate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->do_route(true);
    return 0;
}

void dialog_control::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CStatic ctrl = lpDrawItemStruct->hwndItem;
    ATL::CString str;
    ctrl.GetWindowTextW(str);

    ////using namespace Gdiplus;
    //Gdiplus::Graphics g(lpDrawItemStruct->hDC);
    //Gdiplus::StringFormat str_format;
    //Gdiplus::Font f(lpDrawItemStruct->hDC);
    //Gdiplus::RectF rect(
    //    lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.top,
    //    lpDrawItemStruct->rcItem.right - lpDrawItemStruct->rcItem.left,
    //    lpDrawItemStruct->rcItem.bottom - lpDrawItemStruct->rcItem.top);
    //Gdiplus::SolidBrush brush(Gdiplus::Color(0, 0, 0));

    //str_format.SetTrimming(Gdiplus::StringTrimmingEllipsisWord);
    //str_format.SetAlignment(Gdiplus::StringAlignmentCenter);

    //COLORREF col = GetBkColor(lpDrawItemStruct->hDC);
    //g.Clear(Gdiplus::Color(GetRValue(col), GetGValue(col), GetBValue(col)));
    //g.DrawString(str, -1, &f, rect, &str_format, &brush);

    // trick to fill rect with hdc's background color
    ExtTextOut(lpDrawItemStruct->hDC, 0, 0, ETO_OPAQUE, &lpDrawItemStruct->rcItem, L"", 0, 0);
    DrawText(
        lpDrawItemStruct->hDC, str, -1, 
        &lpDrawItemStruct->rcItem, 
        DT_WORDBREAK | DT_END_ELLIPSIS | DT_CENTER | DT_EDITCONTROL | DT_NOPREFIX);
}

LRESULT dialog_control::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(this->parent.parent.parent.IsIconic())
        return 0;

    // try to find an active audio session in the control and an audio meter for it
    float peak_meter_value = 0.0f;
    for(size_t i = 0; i < this->audio_sessions.size(); i++)
    {
        AudioSessionState state;
        if(this->audio_sessions[i]->GetState(&state) == S_OK)
        {
            if(state == AudioSessionStateActive && this->audio_meters[i] != NULL)
            {
                audio_meters_t::reference peak_meter = this->audio_meters[i];
                peak_meter->GetPeakValue(&peak_meter_value);
                break;
            }
        }
    }

    this->peak_meter_velocity += (1.f / 300.f) / (100.f / (float)TIMER_INTERVAL_CONTROL_DLG);
    this->peak_meter_value -= this->peak_meter_velocity;
    if(peak_meter_value >= this->peak_meter_value)
    {
        this->peak_meter_value = peak_meter_value;
        this->peak_meter_velocity = 0.0f;
    }

    const float slider_level = (float)(100 - this->ctrl_slider.GetPos()) / 100.f;
    const INT peak_indicator_height = 3;

    using namespace Gdiplus;
    const INT w = this->img->GetWidth(), h = this->img->GetHeight();
    Graphics g(this->img.get());
    g.Clear(Color::Transparent);

    {
        const INT h_min = h - (INT)((float)h * peak_meter_value * slider_level);
        const INT h_height = h - h_min;
        LinearGradientBrush brush(
            Rect(0, h_min, w,
            h_height), Color(150, 0, 255, 0), Color(255, 0, 100, 0), 
            LinearGradientModeVertical);
        g.FillRectangle(&brush, Rect(0, h_min + 1, w, h_height));
    }
    {
        LinearGradientBrush brush(
            Rect(0, 0, w, h), Color(255, 255, 0, 0), Color(255, 100, 0, 0), 
            LinearGradientModeHorizontal);
        g.FillRectangle(&brush, Rect(
            0, h - (INT)((float)h * this->peak_meter_value * slider_level) - peak_indicator_height, 
            w, peak_indicator_height));

    }

    // item prepaint event is not fired for slider control with normal invalidate function
    this->ctrl_slider.SetTicFreq(1);

    return 0;
}

LRESULT dialog_control::OnPopUpSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if(this->MessageBoxW(
        L"This will add a new saved routing for this application. It will not " \
        L"replace the old saved routings for this application. Do you wish to continue?",
        L"Confirmation for Save", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return 0;

    bootstrapper& bootstrap = *this->parent.parent.parent.bootstrap;

    input_dialog input_dlg(2);
    input_dlg.DoModal(*this);
    if(IsBadReadPtr(this, sizeof(dialog_control)))
        return 0;
    const int sel_index = input_dlg.selected_index;
    if(sel_index > 0)
    {
        app_inject::devices_t devices;
        app_inject::get_devices(devices);
        try
        {
            bootstrap.save_routing(this->pid, devices[sel_index - 1]);
        }
        catch(std::wstring err)
        {
            app_inject::clear_devices(devices);
            this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
            return 0;
        }
        app_inject::clear_devices(devices);

        dialog_main& main = this->parent.parent;
        for(dialog_main::dialog_arrays_t::iterator it = main.dialog_arrays.begin();
            it != main.dialog_arrays.end();
            it++)
        {
            for(dialog_array::dialog_controls_t::iterator jt = (*it)->dialog_controls.begin();
                jt != (*it)->dialog_controls.end();
                jt++)
            {
                (*jt)->managed = bootstrap.is_saved_routing((*jt)->pid, (*jt)->parent.get_device());
                (*jt)->update_attributes();
            }
        }

        this->MessageBoxW(L"Routing saved. Restart the application for the new settings " \
            L"to take effect.", 
            L"Success", MB_ICONINFORMATION);
    }
    else if(sel_index == 0)
        this->MessageBoxW(L"You cannot save routing to the default device!", 
        NULL, MB_ICONERROR);

    return 0;
}


LRESULT dialog_control::OnPopUpDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    bootstrapper& bootstrap = *this->parent.parent.parent.bootstrap;

    if(this->MessageBoxW(
        L"This will delete all the saved routings for this application. It won't " \
        L"delete the routings for any other application. Proceed?",
        L"Confirmation for Delete All", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
        return 0;

    try
    {
        if(!bootstrap.delete_all(this->pid))
        {
            this->MessageBoxW(
                L"This application doesn't have any saved routings.", NULL, MB_ICONERROR);
            return 0;
        }
    }
    catch(std::wstring err)
    {
        this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
        return 0;
    }

    dialog_main& main = this->parent.parent;
    for(dialog_main::dialog_arrays_t::iterator it = main.dialog_arrays.begin();
        it != main.dialog_arrays.end();
        it++)
    {
        for(dialog_array::dialog_controls_t::iterator jt = (*it)->dialog_controls.begin();
            jt != (*it)->dialog_controls.end();
            jt++)
        {
            (*jt)->managed = bootstrap.is_saved_routing((*jt)->pid, (*jt)->parent.get_device());
            (*jt)->update_attributes();
        }
    }

    this->MessageBoxW(L"Saved routings for this application deleted. Restart " \
        L"the application for the new settings to take effect.", 
        L"Success", MB_ICONINFORMATION);

    return 0;
}
