static std::wstring startupTaskName() {
    HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return L"";
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<BYTE> buffer(size);
    if(!GetTokenInformation(token,TokenUser,buffer.data(),size,&size)){CloseHandle(token);return L"";}CloseHandle(token);
    LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid))return L"";
    std::wstring name=L"corner-"+std::wstring(sid);LocalFree(sid);return name;
}
struct StartupString { BSTR value;explicit StartupString(const std::wstring& s):value(SysAllocString(s.c_str())){}~StartupString(){SysFreeString(value);}operator BSTR()const{return value;} };
static HRESULT createStartupTask(ITaskService* service,ITaskFolder* folder,BSTR name,IRegisteredTask** result) {
    ITaskDefinition* definition=nullptr;IPrincipal* principal=nullptr;ITaskSettings* options=nullptr;ITriggerCollection* triggers=nullptr;ITrigger* trigger=nullptr;ILogonTrigger* logon=nullptr;IActionCollection* actions=nullptr;IAction* action=nullptr;IExecAction* exec=nullptr;
    std::wstring fullName=startupTaskName();std::wstring sid=fullName.substr(7);StartupString user(sid),path(exePath()),arguments(L"--supervise --rounded --external"),working(exePath().substr(0,exePath().find_last_of(L"\\/"))),zero(L"PT0S"),minute(L"PT1M");
    HRESULT hr=service->NewTask(0,&definition);
    if(SUCCEEDED(hr))hr=definition->get_Principal(&principal);
    if(SUCCEEDED(hr))hr=principal->put_UserId(user);
    if(SUCCEEDED(hr))hr=principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if(SUCCEEDED(hr))hr=principal->put_RunLevel(TASK_RUNLEVEL_LUA);
    if(SUCCEEDED(hr))hr=definition->get_Settings(&options);
    if(SUCCEEDED(hr))hr=options->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    if(SUCCEEDED(hr))hr=options->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    if(SUCCEEDED(hr))hr=options->put_ExecutionTimeLimit(zero);
    if(SUCCEEDED(hr))hr=options->put_RestartInterval(minute);
    if(SUCCEEDED(hr))hr=options->put_RestartCount(999);
    if(SUCCEEDED(hr))hr=options->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
    if(SUCCEEDED(hr))hr=definition->get_Triggers(&triggers);
    if(SUCCEEDED(hr))hr=triggers->Create(TASK_TRIGGER_LOGON,&trigger);
    if(SUCCEEDED(hr))hr=trigger->QueryInterface(IID_ILogonTrigger,reinterpret_cast<void**>(&logon));
    if(SUCCEEDED(hr))hr=logon->put_UserId(user);
    if(SUCCEEDED(hr))hr=definition->get_Actions(&actions);
    if(SUCCEEDED(hr))hr=actions->Create(TASK_ACTION_EXEC,&action);
    if(SUCCEEDED(hr))hr=action->QueryInterface(IID_IExecAction,reinterpret_cast<void**>(&exec));
    if(SUCCEEDED(hr))hr=exec->put_Path(path);
    if(SUCCEEDED(hr))hr=exec->put_Arguments(arguments);
    if(SUCCEEDED(hr))hr=exec->put_WorkingDirectory(working);
    VARIANT owner{},empty{};VariantInit(&owner);VariantInit(&empty);owner.vt=VT_BSTR;owner.bstrVal=SysAllocString(sid.c_str());
    if(SUCCEEDED(hr))hr=folder->RegisterTaskDefinition(name,definition,TASK_CREATE_OR_UPDATE,owner,empty,TASK_LOGON_INTERACTIVE_TOKEN,empty,result);
    VariantClear(&owner);
    if(exec)exec->Release();
    if(action)action->Release();
    if(actions)actions->Release();
    if(logon)logon->Release();
    if(trigger)trigger->Release();
    if(triggers)triggers->Release();
    if(options)options->Release();
    if(principal)principal->Release();
    if(definition)definition->Release();
    return hr;
}
static HRESULT startupOperation(int action,bool* enabled=nullptr) {
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return initialized;
    ITaskService* service=nullptr;ITaskFolder* folder=nullptr;IRegisteredTask* task=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_ITaskService,reinterpret_cast<void**>(&service));
    VARIANT empty{};VariantInit(&empty);
    if(SUCCEEDED(hr))hr=service->Connect(empty,empty,empty,empty);
    BSTR root=SysAllocString(L"\\"),name=SysAllocString(startupTaskName().c_str());
    if(SUCCEEDED(hr))hr=service->GetFolder(root,&folder);
    if(SUCCEEDED(hr)) {
        hr=folder->GetTask(name,&task);
        if(FAILED(hr)&&action==1)hr=createStartupTask(service,folder,name,&task);
        else if(FAILED(hr)&&action==0)hr=S_OK;
    }
    if(SUCCEEDED(hr)&&task) {
        if(action>=0)hr=task->put_Enabled(action?VARIANT_TRUE:VARIANT_FALSE);
        else {VARIANT_BOOL value=VARIANT_FALSE;hr=task->get_Enabled(&value);if(enabled)*enabled=value!=VARIANT_FALSE;}
    }
    SysFreeString(name);SysFreeString(root);
    if(task)task->Release();
    if(folder)folder->Release();
    if(service)service->Release();
    if(SUCCEEDED(initialized))CoUninitialize();
    return hr;
}
static HRESULT setStartup(bool enable){return startupOperation(enable?1:0);}
static bool startupEnabled(){bool enabled=false;startupOperation(-1,&enabled);return enabled;}
