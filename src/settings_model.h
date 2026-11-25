struct Settings {
    bool enabled=true,top=true,bottom=true,white=false,startup=true,hideFullscreen=false;
    int radius=40,up=16,down=16,theme=0;
};
static Settings settings;
static std::wstring settingsFileOverride;
static std::wstring exePath();
static int radiusFor(int percent){return std::max(1,MulDiv(std::clamp(percent,0,100),256,100));}
static void normalize(Settings& s) {
    s.up=std::clamp(s.up,0,100);s.down=std::clamp(s.down,0,100);s.theme=std::clamp(s.theme,0,2);
    if(!s.up)s.top=false;
    if(!s.down)s.bottom=false;
    s.radius=std::max(radiusFor(s.up),radiusFor(s.down));
}
static bool sameSettings(const Settings& a,const Settings& b) {
    return a.enabled==b.enabled&&a.top==b.top&&a.bottom==b.bottom&&a.white==b.white&&a.startup==b.startup&&a.hideFullscreen==b.hideFullscreen&&a.up==b.up&&a.down==b.down&&a.theme==b.theme;
}
static std::wstring settingsPath() {if(!settingsFileOverride.empty())return settingsFileOverride;auto p=exePath();return p.substr(0,p.find_last_of(L"\\/"))+L"\\settings.ini";}
static Settings readSettings(int fallback=40) {
    auto path=settingsPath();Settings s;
    s.enabled=GetPrivateProfileIntW(L"Corners",L"Enabled",1,path.c_str())!=0;
    s.top=GetPrivateProfileIntW(L"Corners",L"Top",1,path.c_str())!=0;
    s.bottom=GetPrivateProfileIntW(L"Corners",L"Bottom",1,path.c_str())!=0;
    s.white=GetPrivateProfileIntW(L"Corners",L"White",0,path.c_str())!=0;
    s.startup=GetPrivateProfileIntW(L"Corners",L"Startup",1,path.c_str())!=0;
    s.hideFullscreen=GetPrivateProfileIntW(L"Corners",L"HideFullscreen",0,path.c_str())!=0;
    int legacy=std::clamp(int(GetPrivateProfileIntW(L"Corners",L"Radius",fallback,path.c_str())),0,256);
    s.up=int(GetPrivateProfileIntW(L"Corners",L"UpPercent",MulDiv(legacy,100,256),path.c_str()));
    s.down=int(GetPrivateProfileIntW(L"Corners",L"DownPercent",MulDiv(legacy,100,256),path.c_str()));
    s.theme=int(GetPrivateProfileIntW(L"Corners",L"Theme",0,path.c_str()));normalize(s);return s;
}
static bool saveSettings(const Settings& input) {
    Settings s=input;normalize(s);auto path=settingsPath(),temp=path+L".tmp";
    std::string data="[Corners]\r\nEnabled="+std::to_string(s.enabled)+"\r\nTop="+std::to_string(s.top)+"\r\nBottom="+std::to_string(s.bottom)+"\r\nWhite="+std::to_string(s.white)+"\r\nRadius="+std::to_string(s.radius)+"\r\nUpPercent="+std::to_string(s.up)+"\r\nDownPercent="+std::to_string(s.down)+"\r\nStartup="+std::to_string(s.startup)+"\r\nHideFullscreen="+std::to_string(s.hideFullscreen)+"\r\nTheme="+std::to_string(s.theme)+"\r\n";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;BOOL ok=WriteFile(file,data.data(),DWORD(data.size()),&written,nullptr);CloseHandle(file);
    return ok&&written==data.size()&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
static bool registryLight(bool apps) {
    DWORD value=1,size=sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",apps?L"AppsUseLightTheme":L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size);
    return value!=0;
}
static bool darkTheme(const Settings& s){return s.theme==2||(s.theme==0&&!registryLight(true));}
