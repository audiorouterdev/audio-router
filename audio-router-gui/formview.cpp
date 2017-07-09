#include "formview.h"
#include "window.h"
#include "policy_config.h"
#include <cstring>
#include <cassert>

#ifndef DISABLE_TELEMETRY
extern telemetry* telemetry_m;
#endif

formview::formview(window& parent) : parent(parent)
{
    /*try
    {
        this->initialize();
    }
    catch(std::wstring err)
    {
        err += L"Forcing not available.";
        this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
    }*/
}

formview::~formview()
{
    app_inject backend;
    bool b = false;
    for(routed_processes_t::iterator it = this->routed_processes.begin();
        it != this->routed_processes.end();
        it++)
    {
        if(!it->second.first.empty())
        {
            b = true;
            try { backend.inject(it->first, it->second.second, 0, app_inject::NONE); } 
            catch(...) {}
        }
    }
    if(b)
        reset_all_devices(true);
}

input_dialog::input_dialog(int mode) : 
    selected_index(-1), forced(true), mode(mode)
{
}

void formview::add_item(const std::wstring& name, const std::wstring& routed_to)
{
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.pszText = const_cast<wchar_t*>(name.c_str());
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    item.iSubItem = 0;
    if(this->listview_m.InsertItem(&item) == -1)
    {
        assert(false);
        return;
    }

    item.pszText = const_cast<wchar_t*>(routed_to.c_str());
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    item.iSubItem = 1;
    if(this->listview_m.SetItem(&item) == -1)
    {
        assert(false);
        return;
    }
}

void formview::refresh_list()
{
    const bool debug_b = this->compatible_apps.populate_list();
    assert(debug_b);

    this->listview_m.DeleteAllItems();
    // TODO: a new process might have same id
    size_t found_items = 0;
    for(app_list::apps_t::iterator it = this->compatible_apps.apps.begin();
        it != this->compatible_apps.apps.end();
        it++)
    {
        std::wstring routed_to;
        routed_processes_t::iterator jt = this->routed_processes.find(it->id);
        if(jt != this->routed_processes.end())
        {
            routed_to = jt->second.first;
            found_items++;
        }
        this->add_item(it->name, routed_to);
    }

    // remove routed applications from list if they are not available anymore
   assert(found_items <= this->routed_processes.size());
   if(found_items < this->routed_processes.size())
   {
       for(auto it = this->routed_processes.begin(); it != this->routed_processes.end();)
       {
           bool found = false;
           for(auto jt = this->compatible_apps.apps.begin(); 
               jt != this->compatible_apps.apps.end(); 
               jt++)
           {
               if(it->first == jt->id)
               {
                   found = true;
                   break;
               }
           }
           if(!found)
           {
               it = this->routed_processes.erase(it);
               if(it == this->routed_processes.end())
                   break;
           }
           else
               it++;
       }
   }
}

void formview::open_dialog()
{
    LVITEMW item;
    WTL::CString item_name;
    // not resetting memory causes kernelbase to crash
    // if opening file with enter key in release mode
    memset(&item, 0, sizeof(item));

    if(!this->listview_m.GetSelectedItem(&item))
        return;

    input_dialog input_dlg;
    input_dlg.DoModal(*this);
    const int sel_index = input_dlg.selected_index;
    const bool forced = input_dlg.forced;
    app_inject::flush_t flush = forced ? app_inject::HARD : app_inject::SOFT;
    if(sel_index >= 0)
    {
    #ifndef DISABLE_TELEMETRY
        if(telemetry_m)
            telemetry_m->update_on_routing();
    #endif

        // ------------preferred method which supports forced injection------------
        //const size_t app_index = this->compatible_apps.apps.size() - item.iItem - 1;
        //const app_list::app_info& app = this->compatible_apps.apps[app_index];
        //app_inject injector;
        //delayed_app_info_t app_info;
        //std::wstring endpointdevice_friendlyname;

        //injector.populate_devicelist();
        //app_info.injector = injector;
        //app_info.device_index = (size_t)sel_index;
        //app_info.app = app;
        //this->refresh_list();
        //try
        //{
        //    if(forced)
        //    {
        //        // TODO: remove force if user decides to route it
        //        this->begin(app_info);
        //        // set names
        //        endpointdevice_friendlyname = L"(delayed until application restart)";
        //    }
        //    else
        //    {
        //        app_info.injector.inject(app_info.app.id, app_info.app.x86, (size_t)sel_index);
        //        // set names
        //        if(sel_index > 0)
        //            endpointdevice_friendlyname = injector.device_names[sel_index - 1];
        //    }
        //}
        //catch(std::wstring err)
        //{
        //    err += L"Router functionality not available.";
        //    this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
        //    return;
        //}

        ////this->listview_m.SetItemText(item.iItem, 1, endpointdevice_friendlyname.c_str());
        //// TODO: refactor the partial union
        //this->routed_processes[app.id].first = endpointdevice_friendlyname;
        //this->routed_processes[app.id].second = app.x86;
        //this->refresh_list();


        // ------------ work-a-round version ------------
        // inject
        const size_t app_index = this->compatible_apps.apps.size() - item.iItem - 1;
        const app_list::app_info& app = this->compatible_apps.apps[app_index];
        app_inject injector;
        try
        {
            injector.populate_devicelist();
            injector.inject(app.id, app.x86, sel_index, flush);
        }
        catch(std::wstring err)
        {
            err += L"Router functionality not available.";
            this->MessageBoxW(err.c_str(), NULL, MB_ICONERROR);
            return;
        }

        // set names
        std::wstring endpointdevice_friendlyname;
        if(sel_index > 0)
            endpointdevice_friendlyname = injector.device_names[sel_index - 1];
        this->listview_m.SetItemText(item.iItem, 1, endpointdevice_friendlyname.c_str());
        // TODO: refactor the partial union
        this->routed_processes[app.id].first = endpointdevice_friendlyname;
        this->routed_processes[app.id].second = app.x86;
        
    }
}


LRESULT formview::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->DlgResize_Init(false, false);

    CRect rect;
    this->parent.GetClientRect(&rect);
    this->listview_m = GET(CListViewCtrl, IDC_LIST1);
    //this->listview_m.SubclassWindow(GET(CListViewCtrl, IDC_LIST2));
    const int vscrlx = GetSystemMetrics(SM_CXVSCROLL);
    this->listview_m.InsertColumn(0, L"Audio-enabled application", LVCFMT_LEFT, rect.Width() - rect.Width() / 2);
    this->listview_m.InsertColumn(1, L"Routed to", LVCFMT_LEFT, rect.Width() / 2 - vscrlx);
    this->listview_m.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    this->refresh_list();

    return 0;
}

LRESULT input_dialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    /*GET(CButton, IDC_CHECK1).EnableWindow(FALSE);
    GET(CButton, IDC_CHECK1).ShowWindow(SW_HIDE);*/
    if(this->mode == 1)
        this->SetWindowTextW(L"Duplicate Audio");
    else if(this->mode == 2)
    {
        this->SetWindowTextW(L"Save Routing");
        GET(CButton, IDC_CHECK1).EnableWindow(FALSE);
        GET(CButton, IDC_CHECK1).ShowWindow(SW_HIDE);
    }

    this->combobox_m = GET(CComboBox, IDC_COMBO1);
    if(this->mode == 0)
        this->combobox_m.AddString(L"Default Audio Device");

    app_inject backend;
    backend.populate_devicelist();
    for(size_t i = 0; i < backend.device_names.size(); i++)
        this->combobox_m.AddString(backend.device_names[i].c_str());

    this->combobox_m.SetCurSel(0);

    return TRUE;
}


LRESULT input_dialog::OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    if(this->mode != 0)
        this->selected_index = this->combobox_m.GetCurSel() + 1;
    else
        this->selected_index = this->combobox_m.GetCurSel();
    this->forced = (GET(CButton, IDC_CHECK1).GetCheck() != BST_CHECKED);
    this->EndDialog(0);
    return 0;
}


LRESULT input_dialog::OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    this->EndDialog(0);
    return 0;
}


LRESULT formview::OnDoubleClick(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/)
{
    this->open_dialog();
    return 0;
}