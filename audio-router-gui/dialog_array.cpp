#include "dialog_array.h"
#include "dialog_main.h"
#include "dialog_control.h"
#include "app_inject.h"
#include "window.h"
#include <shellapi.h>
#include <cassert>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include "policy_config.h"

#define AUDCLNT_S_NO_SINGLE_PROCESS AUDCLNT_SUCCESS(0x00d)
#define PROCESS_ACCESS_LEVEL PROCESS_ALL_ACCESS

// https://github.com/sgiurgiu/DefaultAudioChanger/blob/master/DefaultAudioChanger/DevicesManager.cpp#L178
UINT ExtractDeviceIcons(LPWSTR iconPath,HICON *iconLarge,HICON *iconSmall)
{
    if(iconPath == NULL)
        return 0;
    TCHAR *filePath;
    int iconIndex = 0;
    WCHAR* token=NULL;
    filePath=wcstok_s(iconPath,L",",&token);
    TCHAR *iconIndexStr=wcstok_s(NULL,L",",&token);
    if(iconIndexStr != NULL)
        iconIndex=_wtoi(iconIndexStr);
    TCHAR filePathExp[MAX_PATH];
    ExpandEnvironmentStrings(filePath,filePathExp,MAX_PATH);
    return ExtractIconEx(filePathExp,iconIndex,iconLarge,iconSmall,1);
}

class dialog_array::session_notifications : public IAudioSessionNotification
{
private:
    LONG m_cRefAll;
    dialog_array& parent;
public:
    session_notifications(dialog_array&);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl *pNewSession);
};

dialog_array::dialog_array(dialog_main& parent) : 
    parent(parent), device(NULL), session_manager(NULL), 
    audio_volume(NULL), default_device(NULL), session_notif(NULL), icon(NULL)
{
}

dialog_array::~dialog_array()
{
    if(this->icon)
        DestroyIcon(this->icon);

    if(this->audio_volume != NULL)
        this->audio_volume->Release();

    if(this->device != NULL)
    {
        // set routings back to default when exiting
        app_inject backend;
        for(auto it = this->dialog_controls.begin();
            it != this->dialog_controls.end();
            it++)
        {
            try { backend.inject((*it)->pid, (*it)->x86, 0, app_inject::NONE); } 
            catch(...) {}
        }
        if(!this->dialog_controls.empty())
            reset_all_devices(true);
    }

    this->clear_dialog_controls();
    if(this->session_manager != NULL)
    {
        if(this->session_notif != NULL)
        {
            this->session_notif->Release();
            if(this->session_manager->UnregisterSessionNotification(this->session_notif) != S_OK)
                this->session_notif->Release();
        }
        this->session_manager->Release();
    }
    if(this->device != NULL)
        this->device->Release();
    if(this->default_device != NULL)
        this->default_device->Release();
}

void dialog_array::clear_dialog_controls()
{
    for(auto it = this->dialog_controls.begin();
        it != this->dialog_controls.end();
        it++)
    {
        delete *it;
    }
    this->dialog_controls.clear();
}

void dialog_array::set_device(device_t device, const std::wstring& device_name)
{
    if(this->audio_volume != NULL)
        this->audio_volume->Release();
    this->audio_volume = NULL;
    if(this->default_device != NULL)
        this->default_device->Release();
    this->default_device = NULL;

    this->device = device;
    this->device_name = device_name;

    this->ctrl_slider.EnableWindow(TRUE);
    this->ctrl_static.SetWindowTextW(this->device_name.c_str());
    this->parent.RedrawWindow(); // for some odd reason redrawing cstatic wont help

    if(this->device != NULL)
    {
        if(this->device->Activate(
            __uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&this->audio_volume) != S_OK)
        {
            SAFE_RELEASE(this->audio_volume);
            this->ctrl_slider.EnableWindow(FALSE);
            return;
        }
    }
    else
    {
        // set audio volume for default device
        IMMDeviceEnumerator* pEnumerator;
        if(CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator) != S_OK)
        {
            SAFE_RELEASE(pEnumerator);
            this->ctrl_slider.EnableWindow(FALSE);
            return;
        }

        IMMDevice* default_endpoint;
        if(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_endpoint) != S_OK)
        {
            SAFE_RELEASE(default_endpoint);
            pEnumerator->Release();
            this->ctrl_slider.EnableWindow(FALSE);
            return;
        }
        this->default_device = default_endpoint;

        if(default_endpoint->Activate(
            __uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&this->audio_volume) != S_OK)
        {
            SAFE_RELEASE(this->audio_volume);
            //default_endpoint->Release();
            pEnumerator->Release();
            this->ctrl_slider.EnableWindow(FALSE);
            return;
        }

        //default_endpoint->Release();
        pEnumerator->Release();
    }

    float level;
    if(this->audio_volume->GetMasterVolumeLevelScalar(&level) == S_OK)
        this->set_volume((int)(level * 100.f), false);

    // add icon
    device_t device_ = this->device != NULL ? this->device : this->default_device;
    if(device_)
    {
        if(this->icon)
            DestroyIcon(this->icon);
        this->icon = NULL;
        {
            IPropertyStore* pProps = NULL;
            if(!SUCCEEDED(device_->OpenPropertyStore(STGM_READ, &pProps)))
                goto cont;
            PROPVARIANT varIconPath;
            PropVariantInit(&varIconPath);
            if(!SUCCEEDED(pProps->GetValue(PKEY_DeviceClass_IconPath, &varIconPath)))
            {
                PropVariantClear(&varIconPath);
                SAFE_RELEASE(pProps);
                goto cont;
            }
            HICON smallicon = NULL, largeicon = NULL; // <- (large is actually small)
            if(ExtractDeviceIcons(varIconPath.pwszVal, &smallicon, &largeicon))
            {
                if(smallicon)
                {
                    if(largeicon)
                        DestroyIcon(largeicon);
                    this->icon = smallicon;
                }
                else if(largeicon)
                    this->icon = largeicon;
                else
                    this->icon = NULL;
            }
            PropVariantClear(&varIconPath);
            SAFE_RELEASE(pProps);
        }
cont:
        this->ctrl_image.SetIcon(this->icon);
    }
}

void dialog_array::set_volume(int level, bool set)
{
    if(level > 100)
        level = 100;
    else if(level < 0)
        level = 0;

    this->ctrl_slider.SetPos(100 - level);
    if(set && this->audio_volume != NULL)
    {
        this->audio_volume->SetMasterVolumeLevelScalar(((float)level) / 100.f, NULL);
    }
}

void dialog_array::refresh_dialog_controls()
{
    this->clear_dialog_controls();

    //this->ctrl_group.SetWindowTextW(device_name.c_str());

    assert(this->session_manager == NULL);
    device_t device = this->device != NULL ? this->device : this->default_device;
    if(device == NULL || device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, 
        NULL, (void**)&this->session_manager) != S_OK)
    {
        SAFE_RELEASE(this->session_manager);
        return;
    }

    if(this->device != NULL)
    {
        assert(this->session_notif == NULL);
        if(this->session_manager->RegisterSessionNotification(
            (this->session_notif = new session_notifications(*this))) != S_OK)
        {
            this->session_notif->Release();
            this->session_manager->Release();
            this->session_manager = NULL;
            this->session_notif = NULL;
            return;
        }

        IAudioSessionEnumerator* pSessionList = NULL;
        IAudioSessionControl* pSessionControl = NULL;
        int cbSessionCount = 0;

        if(this->session_manager->GetSessionEnumerator(&pSessionList) != S_OK)
        {
            SAFE_RELEASE(pSessionList);
            return;
        }
        if(pSessionList->GetCount(&cbSessionCount) != S_OK)
        {
            pSessionList->Release();
            return;
        }

        for(int i = 0; i < cbSessionCount; i++)
        {
            if(pSessionList->GetSession(i, &pSessionControl) != S_OK)
            {
                SAFE_RELEASE(pSessionControl);
                continue;
            }

            this->choose_array_and_create_control(pSessionControl);
        }

        pSessionList->Release();
    }
}

void dialog_array::reposition_dialog_controls()
{
    RECT rc = {0, 0, width, height};
    for(dialog_controls_t::iterator it = this->dialog_controls.begin();
        it != this->dialog_controls.end();
        it++)
    {
        // -1 for y because borders
        (*it)->SetWindowPos(
            NULL, rc.right - 2, -1, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);

        // getclientrect returns slightly smaller area since the dialogs
        // use border style
        RECT rc2;
        (*it)->GetClientRect(&rc2);
        rc.right += width - 1;//rc2.right + 1;
    }

    this->SetWindowPos(
        NULL, 0, 0, 
        rc.right, rc.bottom, 
        SWP_NOMOVE | SWP_NOZORDER);
    this->ctrl_splitter.ShowWindow(this->dialog_controls.empty() ? SW_HIDE : SW_SHOW);
}

dialog_array::dialog_controls_t::iterator dialog_array::find_control_it(DWORD pid)
{
    for(dialog_controls_t::iterator it = this->dialog_controls.begin();
        it != this->dialog_controls.end();
        it++)
    {
        if((*it)->pid == pid)
            return it;
    }
    return this->dialog_controls.end();
}

dialog_control* dialog_array::find_control(DWORD pid)
{
    dialog_controls_t::iterator it = this->find_control_it(pid);
    return (it != this->dialog_controls.end() ? *it : NULL);
}

dialog_control* dialog_array::create_control(
    DWORD pid, IAudioSessionControl2* audio_session, bool reposition)
{
    // create control and add audio session
    dialog_controls_t::iterator it = this->find_control_it(pid);
    if(it == this->dialog_controls.end())
    {
        dialog_control* new_control = new dialog_control(*this, pid);
        new_control->Create(this->m_hWnd);
        if(audio_session != NULL)
            new_control->add_audio_session(audio_session);
        new_control->set_display_name();
        this->dialog_controls.push_back(new_control);
        if(reposition)
        {
            this->reposition_dialog_controls();
            this->parent.reposition_dialog_arrays();
        }

        return new_control;
    }

    if(audio_session != NULL)
        (*it)->add_audio_session(audio_session);
    (*it)->set_display_name();
    return *it;
}

bool dialog_array::delete_control(DWORD pid, bool reposition)
{
    // TODO: deleting control unsafe because the dialog proc might have a blocking
    // procedure running
    dialog_controls_t::iterator it = this->find_control_it(pid);
    if(it != this->dialog_controls.end())
    {
        (*it)->DestroyWindow();
        delete *it;
        this->dialog_controls.erase(it);
        if(reposition)
        {
            this->reposition_dialog_controls();
            this->parent.reposition_dialog_arrays();
        }

        return true;
    }

    return false;
}

bool dialog_array::take_control(DWORD pid, dialog_array* arr)
{
    dialog_control* control = arr->find_control(pid);
    if(control == NULL)
        return false;

    // copy mute state from old control
    const bool mute_state = control->is_muted();
    // copy sessions from old control and delete it
    dialog_control::audio_sessions_t audio_sessions;
    for(dialog_control::audio_sessions_t::const_iterator it = control->get_audio_sessions().begin();
        it != control->get_audio_sessions().end();
        it++)
    {
        AudioSessionState state = AudioSessionStateExpired;
        (*it)->GetState(&state);
        // seems like firefox doesn't release the old audio clients
        // so the old sessions don't expire or a session doesn't expire
        // if there's a session control attached to it
        if(state != AudioSessionStateExpired)
        {
            audio_sessions.push_back(*it);
            (*it)->AddRef();
        }
    }
    arr->delete_control(pid);
    
    // create new control and copy audio sessions and mute state
    dialog_control* new_control = this->create_control(pid, NULL);
    new_control->set_mute(mute_state);
    for(dialog_control::audio_sessions_t::iterator it = audio_sessions.begin();
        it != audio_sessions.end();
        it++)
        new_control->add_audio_session(*it);

    return true;
}

void dialog_array::update_controls()
{
    if(this->dialog_controls.empty())
        return;

    bool control_deleted = false;
    typedef std::vector<DWORD> pids_t;
    pids_t pids;
    for(dialog_controls_t::iterator it = this->dialog_controls.begin();
        it != this->dialog_controls.end();
        it++)
    {
        HANDLE hproc = OpenProcess(PROCESS_ACCESS_LEVEL, FALSE, (*it)->pid);
        if(hproc == NULL)
        {
            pids.push_back((*it)->pid);
            continue;
        }

        DWORD exitcode;
        if(!GetExitCodeProcess(hproc, &exitcode) || exitcode != STILL_ACTIVE)
        {
            pids.push_back((*it)->pid);
            CloseHandle(hproc);
            continue;
        }

        CloseHandle(hproc);

        // update names aswell
        (*it)->set_display_name(false);
    }

    for(pids_t::iterator it = pids.begin(); it != pids.end(); it++)
    {
        control_deleted = this->delete_control(*it, false);
        assert(control_deleted);
    }

    if(control_deleted)
    {
        this->reposition_dialog_controls();
        this->parent.reposition_dialog_arrays();
    }

    //// delete old sessions
    //for(dialog_controls_t::iterator it = this->dialog_controls.begin();
    //    it != this->dialog_controls.end();
    //    it++)
    //{
    //    it->second->delete_audio_sessions();
    //}

    //bool control_deleted = false;
    //IAudioSessionEnumerator* pSessionList = NULL;
    //if(this->session_manager == NULL || 
    //    this->session_manager->GetSessionEnumerator(&pSessionList) != S_OK)
    //{
    //    SAFE_RELEASE(pSessionList);
    //    return;
    //}

    //int cbSessionCount = 0;
    //if(pSessionList->GetCount(&cbSessionCount) != S_OK)
    //{
    //    pSessionList->Release();
    //    return;
    //}

    //// add new sessions to controls
    //for(int i = 0; i < cbSessionCount; i++)
    //{
    //    IAudioSessionControl* pSessionControl = NULL;
    //    if(pSessionList->GetSession(i, &pSessionControl) != S_OK)
    //    {
    //        SAFE_RELEASE(pSessionControl);
    //        continue;
    //    }

    //    IAudioSessionControl2* pSessionControl2 = NULL;
    //    const DWORD pid = 
    //        dialog_array::get_audio_session_control(pSessionControl, &pSessionControl2);
    //    if(pid == 0)
    //    {
    //        pSessionControl->Release();
    //        continue;
    //    }

    //    dialog_controls_t::iterator it = this->dialog_controls.find(pid);
    //    if(it != this->dialog_controls.end())
    //        it->second->add_audio_session(pSessionControl2);
    //    else
    //        pSessionControl2->Release();

    //    pSessionControl->Release();
    //}

    //// check controls that don't have sessions
    //typedef std::vector<DWORD> pids_t;
    //pids_t pids;
    //for(dialog_controls_t::iterator it = this->dialog_controls.begin();
    //    it != this->dialog_controls.end();
    //    it++)
    //{
    //    if(it->second->get_audio_sessions().empty())
    //        pids.push_back(it->second->pid);
    //}

    //// delete controls that don't have sessions
    //for(pids_t::iterator it = pids.begin(); it != pids.end(); it++)
    //{
    //    control_deleted = this->delete_control(*it, false);
    //    assert(control_deleted);
    //}

    //pSessionList->Release();

    //if(control_deleted)
    //{
    //    this->reposition_dialog_controls();
    //    this->parent.reposition_dialog_arrays();
    //}
}

DWORD dialog_array::get_audio_session_control(IAudioSessionControl* in, IAudioSessionControl2** out)
{
    if(in->QueryInterface(__uuidof(IAudioSessionControl2), (void**)out) != S_OK)
    {
        SAFE_RELEASE((*out));
        return 0;
    }

    // TODO: decide if show dummy dialog control if audio control of session fails
    DWORD pid;
    HRESULT hr = (*out)->GetProcessId(&pid);
    if(hr != S_OK && hr != AUDCLNT_S_NO_SINGLE_PROCESS)
    {
        (*out)->Release();
        return 0;
    }

    // check if the pid actually exists(ghost sessions)
    HANDLE hproc = OpenProcess(PROCESS_ACCESS_LEVEL, FALSE, pid);
    if(hproc == NULL)
    {
        DWORD err = GetLastError();
        (*out)->Release();
        return 0;
    }
    CloseHandle(hproc);

    return pid;
}

void dialog_array::choose_array_and_create_control(IAudioSessionControl* pSessionControl)
{
    IAudioSessionControl2* pSessionControl2 = NULL;
    const DWORD pid = get_audio_session_control(pSessionControl, &pSessionControl2);
    pSessionControl->Release();
    if(pid == 0)
    {
        // TODO: decide if show dummy session
        return;
    }

    // check if control is requesting routing
    {
        dialog_control* control;
        dialog_array* arr = this->parent.find_control(pid, &control, 0);
        for(size_t i = 0; arr != NULL; arr = this->parent.find_control(pid, &control, ++i))
        {
            if(control->routing_state == dialog_control::ROUTING)
            {
                // special case if the new route has the same destination
                // (otherwise the logic would take the control twice)
                if(&control->parent == this)
                    this->take_control(pid, arr);   

                // routing state found; take controls from all other arrays
                for(auto it = this->parent.dialog_arrays.begin();
                    it != this->parent.dialog_arrays.end();
                    it++)
                {
                    dialog_control* control = (*it)->find_control(pid);
                    if(control && *it != this)
                        this->take_control(pid, *it);
                }
                
                // add new audio session
                this->find_control(pid)->add_audio_session(pSessionControl2);

                return;
            }
        }
    }

    // check if control is found
    {
        dialog_control* control;
        dialog_array* arr = this->parent.find_control(pid, &control, 0);
        bootstrapper& bootstrap = *this->parent.parent.bootstrap;

        // check if control is found on default: add session if found;
        // if not found on default: add to this if found in general; add to default if not found in general
        // (duplication option only available on routed processes);
        // also add to this if it's a managed app
        if(arr == this->parent.dialog_arrays[0])
        {
            control->add_audio_session(pSessionControl2);
            return;
        }
        else if(arr != NULL || bootstrap.is_managed_app(pid))
        {
            this->create_control(pid, pSessionControl2);
            return;
        }
        else
        {
            this->parent.dialog_arrays[0]->create_control(pid, pSessionControl2);
            return;
        }
    }

    assert(false);
    pSessionControl2->Release();
}

LRESULT dialog_array::OnSessionCreated(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // TODO: default audio device will be renamed to unmanaged apps in future
    if(wParam == NULL && lParam != NULL)
    {
        dialog_control* control = (dialog_control*)lParam;
        //dialog_array* arr = &control->parent;
        const DWORD pid = control->pid;

        // take controls from other arrays back to default
        for(auto it = this->parent.dialog_arrays.begin();
            it != this->parent.dialog_arrays.end();
            it++)
        {
            if(it != this->parent.dialog_arrays.begin())
            {
                dialog_control* control = (*it)->find_control(pid);
                if(control)
                    this->parent.dialog_arrays[0]->take_control(pid, *it);
            }
        }

        return 0;
    }

    IAudioSessionControl* pSessionControl = (IAudioSessionControl*)wParam;
    this->choose_array_and_create_control(pSessionControl);

    return 0;
}

LRESULT dialog_array::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    {
        RECT rc = {spacing_x, spacing_y, width, height};
        this->MapDialogRect(&rc);
        const_cast<int&>(width) = rc.right;
        const_cast<int&>(height) = rc.bottom;
        const_cast<int&>(spacing_x) = rc.left;
        const_cast<int&>(spacing_y) = rc.top;
    }
    /*this->DlgResize_Init(false, false);*/

    // retrieve items
    RECT rc;
    this->ctrl_group = this->GetDlgItem(IDC_GROUP1);
    this->ctrl_button = this->GetDlgItem(IDC_BUTTON1);
    this->ctrl_image = this->GetDlgItem(IDC_IMAGE);
    this->ctrl_slider = this->GetDlgItem(IDC_SLIDER1);
    this->ctrl_static = this->GetDlgItem(IDC_STATIC1);
    this->ctrl_static.SetWindowLongW(
        GWL_STYLE, this->ctrl_static.GetWindowLongW(GWL_STYLE) | SS_OWNERDRAW);

    //this->ctrl_group2.Create(this->parent.m_hWnd, NULL, NULL, BS_GROUPBOX | WS_CHILD);

    // set dialog
    this->GetClientRect(&rc);
    //this->ctrl_group.SetWindowTextW(L"Default Audio Device");
    //this->SetWindowPos(NULL, 0, 0, rc.right + 100, rc.bottom, SWP_NOZORDER);

    // set splitter
    // TODO: set splitter the same way groupbox is set
    RECT ctrl_splitter_rc;
    // TODO: delete the additional -2's from here
    ctrl_splitter_rc.left = rc.right/* - 2*/;
    ctrl_splitter_rc.right = rc.right + 1/* - 2*/;
    this->ctrl_image.GetWindowRect(&rc);
    this->ScreenToClient(&rc);
    ctrl_splitter_rc.top = rc.top;
    this->ctrl_button.GetWindowRect(&rc);
    this->ScreenToClient(&rc);
    ctrl_splitter_rc.bottom = rc.bottom;
    this->ctrl_splitter.Create(
        this->m_hWnd, ctrl_splitter_rc, NULL, SS_ETCHEDVERT | WS_CHILD);

    // set button
    this->ctrl_button.SetWindowTextW(L"Mute");
    this->ctrl_group.SetParent(this->parent.m_hWnd);
    this->ctrl_group.ShowWindow(SW_HIDE);

    // set slider control
    this->ctrl_slider.SetRange(0, 100);
    this->ctrl_slider.SetPos(0);
    LONG style = this->ctrl_slider.GetWindowLongW(GWL_STYLE);
    style |= TBS_NOTIFYBEFOREMOVE;
    this->ctrl_slider.SetWindowLongW(GWL_STYLE, style);

    // promote the controls in this dialog to be child controls
    // of the parent dialog
    style = this->GetWindowLongW(GWL_EXSTYLE);
    style |= WS_EX_CONTROLPARENT;
    this->SetWindowLongW(GWL_EXSTYLE, style);
    /*CDialogMessageHook::InstallHook(*this);*/

    return TRUE;
}

dialog_array::session_notifications::session_notifications(dialog_array& parent) :
    parent(parent), m_cRefAll(1)
{
}

HRESULT dialog_array::session_notifications::QueryInterface(REFIID riid, void **ppvInterface)
{
    if(IID_IUnknown == riid)
    {
        this->AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if(__uuidof(IAudioSessionNotification) == riid)
    {
        this->AddRef();
        *ppvInterface = (IAudioSessionNotification*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}

ULONG dialog_array::session_notifications::AddRef()
{
    return InterlockedIncrement(&this->m_cRefAll);
}

ULONG dialog_array::session_notifications::Release()
{
    ULONG ulRef = InterlockedDecrement(&this->m_cRefAll);
    if(ulRef == 0)
    {
        delete this;
    }
    return ulRef;
}

HRESULT dialog_array::session_notifications::OnSessionCreated(IAudioSessionControl *pNewSession)
{
    if(pNewSession)
    {
        pNewSession->AddRef();
        ::PostMessage(this->parent.m_hWnd, WM_SESSION_CREATED, (WPARAM)pNewSession, 0);
    }
    return S_OK;
}

LRESULT dialog_array::OnVolumeChange(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/)
{
    // This feature requires Windows Vista or greater.
    // The symbol _WIN32_WINNT must be >= 0x0600.
    NMTRBTHUMBPOSCHANGING *pNMTPC = reinterpret_cast<NMTRBTHUMBPOSCHANGING *>(pNMHDR);

    this->set_volume(100 - (int)pNMTPC->dwPos);

    return 0;
}

LRESULT dialog_array::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /*CDialogMessageHook::UninstallHook(*this);*/
    return 0;
}

LRESULT dialog_array::OnBnClickedButton1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if(this->audio_volume != NULL)
    {
        BOOL mute;
        if(this->audio_volume->GetMute(&mute) == S_OK)
        {
            if(mute)
            {
                if(this->audio_volume->SetMute(FALSE, NULL) == S_OK)
                {
                    this->ctrl_button.SetWindowTextW(L"Mute");
                    this->ctrl_slider.EnableWindow(TRUE);
                }
            }
            else
            {
                if(this->audio_volume->SetMute(TRUE, NULL) == S_OK)
                {
                    this->ctrl_button.SetWindowTextW(L"Unmute");
                    this->ctrl_slider.EnableWindow(FALSE);
                }
            }
        }
    }

    return 0;
}

void dialog_array::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CStatic ctrl = lpDrawItemStruct->hwndItem;
    ATL::CString str;
    ctrl.GetWindowTextW(str);
    DrawText(
        lpDrawItemStruct->hDC, str, -1, 
        &lpDrawItemStruct->rcItem, DT_WORDBREAK | DT_END_ELLIPSIS | DT_CENTER | DT_EDITCONTROL | DT_NOPREFIX);
}
