#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <servprov.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <commctrl.h>

#include <taskschd.h>
#include <sddl.h>
#include "settings_model.h"
#include "startup.h"
#include "fonts.h"
static int showSettings();
static void openSettings();
static HICON brandIcon();
static UINT showTrayPopup(HWND owner);

// No animation loop, polling timer, keyboard hook, service, or managed runtime.
struct Panel { std::wstring device, path, name; RECT rect{}; bool internal=false; };
struct Mask { int w=0,h=0; std::vector<BYTE> alpha; };
struct State {
    DWORD version=1, pid=0, supervisor=0, capture=0, debug=0, visible=0;
    LONG events=0, renders=0, changes=0, pin=0;
    int x=0,y=0,width=0,height=0;
    DWORD rounded=0,cornerSize=0;
    DWORD enabled=1,top=1,bottom=1,white=0;
    LONG layerRepairs=0;
    DWORD up=0,down=0,startup=1,theme=0;
    DWORD preview=0;
    DWORD hideFullscreen=0;
};
static State* state=nullptr;
static HWND controller=nullptr, corners[4]{};
static int cornerCount=2;
static Panel selected;
static Mask masks[4];
static bool debugMode=false, demo=false, rounded=false, allowExternal=false, noPin=false, panelPresent=false;
static int cornerSize=40;
static DWORD parentPid=0;
static std::wstring panelPath,leftFile,rightFile,report;
static int expectedW=0,expectedH=0;
static HANDLE stopping=nullptr;
static HANDLE previewOwner=nullptr;
static UINT shellMessage=0,taskbarMessage=0;
static constexpr UINT Refresh = WM_APP+1;
static constexpr UINT ReloadSettings = WM_APP+2;
static constexpr UINT TrayMessage = WM_APP+3;
static NOTIFYICONDATAW tray{};
static const wchar_t* appId=L"corner.Local.1";
static std::wstring objectName(const wchar_t* suffix) {
    DWORD session=0; ProcessIdToSessionId(GetCurrentProcessId(),&session);
    return L"Local\\GlassCorners."+std::to_wstring(session)+L"."+suffix;
}
static std::string utf8(const std::wstring& s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string r(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),r.data(),n,nullptr,nullptr); return r;
}
static std::string quoted(const std::wstring& s) {
    std::string r="\"";
    for (unsigned char c:utf8(s)) {
        if(c=='"'||c=='\\') {r+='\\';r+=c;}
        else if(c<32) {char b[7]; wsprintfA(b,"\\u%04x",c);r+=b;}
        else r+=c;
    } return r+'"';
}
static void output(const std::string& text) {
    if(!report.empty()) {
        HANDLE f=CreateFileW(report.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot write report");
        DWORD written=0; BOOL ok=WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr); CloseHandle(f);
        if(!ok||written!=text.size()) throw std::runtime_error("Report write failed");
    } else {
        DWORD written=0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),DWORD(text.size()),&written,nullptr);
    }
}
static std::vector<Panel> panels() {
    UINT pc=0,mc=0; LONG rc=0;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    for(int attempt=0;attempt<4;attempt++) {
        if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pc,&mc)!=ERROR_SUCCESS)
            throw std::runtime_error("Display topology unavailable");
        paths.resize(pc); modes.resize(mc);
        rc=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pc,paths.data(),&mc,modes.data(),nullptr);
        if(rc!=ERROR_INSUFFICIENT_BUFFER) break;
    }
    if(rc!=ERROR_SUCCESS) throw std::runtime_error("QueryDisplayConfig failed");
    std::vector<Panel> result;
    for(UINT i=0;i<pc;i++) {
        auto& p=paths[i];
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),p.sourceInfo.adapterId,p.sourceInfo.id};
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header={DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,sizeof(target),p.targetInfo.adapterId,p.targetInfo.id};
        if(DisplayConfigGetDeviceInfo(&source.header)||DisplayConfigGetDeviceInfo(&target.header)) continue;
        Panel q; q.device=source.viewGdiDeviceName;q.path=target.monitorDevicePath;q.name=target.monitorFriendlyDeviceName;
        auto tech=p.targetInfo.outputTechnology;
        q.internal=tech==DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL||tech==DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED||tech==DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED;
        struct Context { const wchar_t* device; RECT rect; bool found; } ctx{q.device.c_str(),{},false};
        EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM data)->BOOL {
            auto& c=*reinterpret_cast<Context*>(data); MONITORINFOEXW info{};info.cbSize=sizeof(info);
            if(GetMonitorInfoW(h,&info)&&!wcscmp(c.device,info.szDevice)){c.rect=info.rcMonitor;c.found=true;}return TRUE;
        },reinterpret_cast<LPARAM>(&ctx));
        if(ctx.found) {q.rect=ctx.rect;result.push_back(q);}
    }
    return result;
}
static std::string panelJSON(const Panel& p) {
    return "{\"device\":"+quoted(p.device)+",\"path\":"+quoted(p.path)+",\"name\":"+quoted(p.name)+
        ",\"internal\":"+(p.internal?"true":"false")+",\"x\":"+std::to_string(p.rect.left)+",\"y\":"+std::to_string(p.rect.top)+
        ",\"width\":"+std::to_string(p.rect.right-p.rect.left)+",\"height\":"+std::to_string(p.rect.bottom-p.rect.top)+"}";
}
static Mask loadMask(const std::wstring& path) {
    Gdiplus::Bitmap bmp(path.c_str());
    if(bmp.GetLastStatus()!=Gdiplus::Ok) throw std::runtime_error("Cannot open mask PNG");
    int w=int(bmp.GetWidth()),h=int(bmp.GetHeight());
    if(w<1||h<1||w>512||h>512) throw std::runtime_error("Mask must be 1..512 physical pixels per side");
    if(!(bmp.GetPixelFormat()&PixelFormatAlpha)) throw std::runtime_error("Mask must have an alpha channel");
    Mask m{w,h,{}};m.alpha.resize(w*h);
    bool visible=false,transparent=false;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        Gdiplus::Color c;
        if(bmp.GetPixel(x,y,&c)!=Gdiplus::Ok) throw std::runtime_error("Cannot decode mask pixel");
        BYTE a=c.GetA();m.alpha[y*w+x]=a;visible|=a>0;transparent|=a==0;
    }
    if(!visible||!transparent) throw std::runtime_error("Mask must include opaque and transparent pixels");
    return m;
}
static Mask fixture(bool right) {
    // Deliberately a staircase, NOT an approximation of any panel or Apple curve.
    Mask m{64,64,{}};m.alpha.resize(64*64);
    for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        int u=right?63-x:x;
        m.alpha[y*64+x]=u<(64-y)/4?255:0;
    }return m;
}
static Mask smoothCorner(bool right,int size) {
    // Explicitly approximate continuous corner. Not an extracted Apple contour.
    // Supersampling preserves a smooth edge without a circular arc.
    Mask m{size,size,{}};m.alpha.resize(size*size);
    constexpr int samples=8;
    std::vector<double> curve(size*samples);
    for(int i=0;i<size*samples;i++)curve[i]=std::pow(1-(i+0.5)/(size*samples),2.6);
    for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
        int covered=0;
        for(int sy=0;sy<samples;sy++)for(int sx=0;sx<samples;sx++) {
            if(curve[x*samples+sx]+curve[y*samples+sy]>1)covered++;
        }
        m.alpha[y*size+(right?size-1-x:x)]=BYTE((covered*255+samples*samples/2)/(samples*samples));
    }return m;
}
static void rebuildRoundedMasks() {
    masks[0]=smoothCorner(false,radiusFor(settings.down));masks[1]=smoothCorner(true,radiusFor(settings.down));
    masks[2]=smoothCorner(false,radiusFor(settings.up));masks[3]=smoothCorner(true,radiusFor(settings.up));
    cornerSize=settings.radius;
}
static void updateStateSettings() {
    state->cornerSize=rounded?cornerSize:0;state->enabled=settings.enabled;state->top=settings.top;state->bottom=settings.bottom;state->white=settings.white;
    state->up=settings.up;state->down=settings.down;state->startup=settings.startup;state->theme=settings.theme;
    state->hideFullscreen=settings.hideFullscreen;
}
static void applyLiveSettings(Settings incoming,bool preview) {
    normalize(incoming);
    if(preview){incoming.startup=settings.startup;incoming.theme=settings.theme;}
    bool downChanged=incoming.down!=settings.down,upChanged=incoming.up!=settings.up;
    settings=incoming;
    if(rounded){
        if(downChanged){masks[0]=smoothCorner(false,radiusFor(settings.down));masks[1]=smoothCorner(true,radiusFor(settings.down));}
        if(upChanged){masks[2]=smoothCorner(false,radiusFor(settings.up));masks[3]=smoothCorner(true,radiusFor(settings.up));}
        cornerSize=settings.radius;
    }
    state->preview=preview;updateStateSettings();
    if(!preview&&previewOwner){CloseHandle(previewOwner);previewOwner=nullptr;}
}
static std::vector<DWORD> pixels(const Mask& m,bool bright,bool flip=true) {
    std::vector<DWORD> p(m.w*m.h);
    for(int y=0;y<m.h;y++)for(int x=0;x<m.w;x++) {
        DWORD a=m.alpha[(flip?m.h-1-y:y)*m.w+x];
        p[y*m.w+x]=(a<<24)|(bright?((a<<16)|a):(settings.white?((a<<16)|(a<<8)|a):0));
    }return p;
}
static bool fullscreen(HWND w) {
    if(!w||!IsWindowVisible(w)||IsIconic(w))return false;
    wchar_t cls[128]{};GetClassNameW(w,cls,128);
    if(!wcscmp(cls,L"Progman")||!wcscmp(cls,L"WorkerW")||!wcscmp(cls,L"Shell_TrayWnd"))return false;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId())return false;
    // Maximized ordinary windows keep captions; do not call these fullscreen.
    if((GetWindowLongPtrW(w,GWL_STYLE)&WS_CAPTION)==WS_CAPTION)return false;
    RECT r{};
    if(FAILED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))&&!GetWindowRect(w,&r))return false;
    const auto& s=selected.rect;
    return r.left<=s.left&&r.top<=s.top&&r.right>=s.right&&r.bottom>=s.bottom;
}
static bool locate() {
    auto list=panels();std::vector<Panel> candidates;
    for(auto& p:list) if((panelPath.empty()?p.internal:p.path==panelPath)&&(p.internal||allowExternal))candidates.push_back(p);
    if((demo||(rounded&&allowExternal))&&panelPath.empty()&&candidates.empty()&&!list.empty())candidates.push_back(list.front());
    if(candidates.size()!=1)return false;
    selected=candidates.front();
    if(!demo&&!rounded&&(selected.rect.right-selected.rect.left!=expectedW||selected.rect.bottom-selected.rect.top!=expectedH))return false;
    return true;
}
static void render(int side) {
    const auto& m=masks[side];auto p=pixels(m,debugMode,side<2);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=m.w;bi.bmiHeader.biHeight=-m.h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* data=nullptr;HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&data,nullptr,0);
    if(!dc||!bitmap||!data){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("Cannot allocate corner bitmap");}
    CopyMemory(data,p.data(),p.size()*sizeof(DWORD));auto old=SelectObject(dc,bitmap);
    POINT dst{side%2?selected.rect.right-m.w:selected.rect.left,side<2?selected.rect.bottom-m.h:selected.rect.top},src{};
    SIZE size{m.w,m.h};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    BOOL ok=UpdateLayeredWindow(corners[side],nullptr,&dst,&size,dc,&src,0,&blend,ULW_ALPHA);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    if(!ok)throw std::runtime_error("UpdateLayeredWindow failed");
    InterlockedIncrement(&state->renders);
}
static bool isTaskbar(HWND w) {
    wchar_t cls[64]{};if(!w)return false;GetClassNameW(w,cls,64);
    return !wcscmp(cls,L"Shell_TrayWnd")||!wcscmp(cls,L"Shell_SecondaryTrayWnd");
}
static bool shellCoversCorners() {
    HWND foreground=GetForegroundWindow();bool protectFullscreen=!settings.hideFullscreen&&fullscreen(foreground);
    for(int i=0;i<cornerCount;i++) {
        HWND corner=corners[i];if(!corner||!IsWindowVisible(corner))continue;
        RECT bounds{};GetWindowRect(corner,&bounds);
        int limit=1024;
        for(HWND above=GetWindow(corner,GW_HWNDPREV);above&&limit--;above=GetWindow(above,GW_HWNDPREV)) {
            if((!isTaskbar(above)&&!(protectFullscreen&&above==foreground))||!IsWindowVisible(above))continue;
            RECT shell{},intersection{};GetWindowRect(above,&shell);
            if(IntersectRect(&intersection,&bounds,&shell))return true;
        }
    }
    return false;
}
static void restoreLayerOrder() {
    // A taskbar can promote itself without changing foreground or corner visibility.
    // Only raise on a real overlap above our windows; own reorders therefore settle.
    if(!shellCoversCorners())return;
    for(int i=0;i<cornerCount;i++)if(corners[i]&&IsWindowVisible(corners[i]))
        SetWindowPos(corners[i],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    InterlockedIncrement(&state->layerRepairs);
}
static void refresh(bool topology=false,bool redraw=false) {
    if(topology) {try{panelPresent=locate();}catch(...){panelPresent=false;}}
    if((topology||redraw)&&panelPresent){for(int i=0;i<cornerCount;i++)render(i);}
    bool show=panelPresent&&settings.enabled&&(!settings.hideFullscreen||!fullscreen(GetForegroundWindow()));
    bool any=show&&(settings.bottom||(cornerCount>2&&settings.top));
    if(any!=bool(state->visible)||topology||redraw) {
        for(int i=0;i<cornerCount;i++) if(HWND w=corners[i]) {
            if(show&&(i<2?settings.bottom:settings.top))SetWindowPos(w,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            else ShowWindow(w,SW_HIDE);
        }
        state->visible=any;InterlockedIncrement(&state->changes);
    }
    if(any)restoreLayerOrder();
}
static void CALLBACK eventHook(HWINEVENTHOOK,DWORD event,HWND w,LONG object,LONG,DWORD,DWORD) {
    if(event==EVENT_OBJECT_REORDER) {
        if(object!=OBJID_WINDOW||!state->visible||!shellCoversCorners())return;
    }else if(event==EVENT_OBJECT_SHOW) {
        if(object!=OBJID_WINDOW||!isTaskbar(w))return;
    }else if(event==EVENT_OBJECT_FOCUS) {
        if(!isTaskbar(GetAncestor(w,GA_ROOT)))return;
    }else if(event==EVENT_OBJECT_LOCATIONCHANGE) {
        if(object!=OBJID_WINDOW||(w!=GetForegroundWindow()&&!isTaskbar(w)))return;
    }else if(event!=EVENT_SYSTEM_FOREGROUND&&event!=EVENT_SYSTEM_MOVESIZEEND&&event!=EVENT_SYSTEM_MINIMIZEEND)return;
    InterlockedIncrement(&state->events);PostMessageW(controller,Refresh,0,0);
}
static LRESULT CALLBACK cornerProc(HWND w,UINT msg,WPARAM a,LPARAM b) {
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;
    if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(msg==WM_ERASEBKGND)return 1;
    return DefWindowProcW(w,msg,a,b);
}
static LRESULT CALLBACK controlProc(HWND w,UINT msg,WPARAM a,LPARAM b) {
    try {
        if(msg==WM_COPYDATA) {
            auto copy=reinterpret_cast<COPYDATASTRUCT*>(b);
            if(copy&&copy->dwData==0x434f524e&&copy->cbData==sizeof(Settings)&&copy->lpData) {
                DWORD pid=0;GetWindowThreadProcessId(reinterpret_cast<HWND>(a),&pid);
                if(previewOwner){CloseHandle(previewOwner);previewOwner=nullptr;}
                if(pid)previewOwner=OpenProcess(SYNCHRONIZE,FALSE,pid);
                Settings incoming=*static_cast<const Settings*>(copy->lpData);applyLiveSettings(incoming,true);refresh(false,true);return 1;
            }
            return 0;
        }
        if(msg==ReloadSettings) {
            applyLiveSettings(readSettings(cornerSize),false);
            refresh(false,true);return 1;
        }
        if(msg==TrayMessage) {
            if(b==WM_LBUTTONUP||b==WM_RBUTTONUP) {
                UINT choice=showTrayPopup(w);
                if(choice==1)openSettings();
                if(choice==2) {
                    settings=readSettings();settings.enabled=!settings.enabled;
                    if(settings.enabled&&!settings.top&&!settings.bottom){settings.top=settings.up>0;settings.bottom=settings.down>0;}
                    if(saveSettings(settings))SendMessageW(w,ReloadSettings,0,0);
                }
                if(choice==3){HWND ui=FindWindowW(L"GlassCorners.Settings",nullptr);if(ui)PostMessageW(ui,WM_CLOSE,0,0);SetEvent(stopping);}
            }
            return 0;
        }
        if(msg==Refresh){refresh();return 0;}
        if(msg==WM_SETTINGCHANGE){HICON old=tray.hIcon;tray.hIcon=brandIcon();Shell_NotifyIconW(NIM_MODIFY,&tray);if(old)DestroyIcon(old);return 0;}
        if(msg==WM_DISPLAYCHANGE||msg==WM_DPICHANGED){refresh(true);return 0;}
        if(msg==taskbarMessage){state->pin=0;Shell_NotifyIconW(NIM_ADD,&tray);refresh(true);return 0;}
        if(msg==shellMessage){if((a&0x7fff)==HSHELL_WINDOWACTIVATED||(a&0x7fff)==HSHELL_WINDOWDESTROYED)refresh();return 0;}
        if(msg==WM_CLOSE){SetEvent(stopping);return 0;}
    }catch(...){for(HWND c:corners)if(c)ShowWindow(c,SW_HIDE);state->visible=0;}
    return DefWindowProcW(w,msg,a,b);
}

// Private COM is isolated in a short-lived process: an ABI break or a hang cannot
// take down or block the renderer. Pinning success does NOT certify animation behavior.
struct PinnedApps: IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsAppIdPinned(PCWSTR,BOOL*)=0;
    virtual HRESULT STDMETHODCALLTYPE PinAppID(PCWSTR)=0;
    virtual HRESULT STDMETHODCALLTYPE UnpinAppID(PCWSTR)=0;
};
static int pinHelper(bool remove=false) {
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 20;
    CLSID shell{},service{};IID iid{};
    CLSIDFromString(L"{C2F03A33-21F5-47FA-B4BB-156362A2F239}",&shell);
    CLSIDFromString(L"{B5A399E7-1C87-46B8-88E9-FC5747B171BD}",&service);
    IIDFromString(L"{4CE81583-1E4C-4632-A621-07A53543148F}",&iid);
    IServiceProvider* provider=nullptr;PinnedApps* api=nullptr;BOOL pinned=FALSE;
    HRESULT hr=CoCreateInstance(shell,nullptr,CLSCTX_LOCAL_SERVER,IID_IServiceProvider,reinterpret_cast<void**>(&provider));
    if(SUCCEEDED(hr))hr=provider->QueryService(service,iid,reinterpret_cast<void**>(&api));
    if(SUCCEEDED(hr))hr=remove?api->UnpinAppID(appId):api->PinAppID(appId);
    if(SUCCEEDED(hr))hr=api->IsAppIdPinned(appId,&pinned);
    if(api)api->Release();
    if(provider)provider->Release();
    CoUninitialize();
    return SUCCEEDED(hr)&&pinned==!remove?0:21;
}
static std::wstring exePath() {wchar_t p[32768]{};DWORD n=GetModuleFileNameW(nullptr,p,32768);return std::wstring(p,n);}
static std::wstring cmdQuote(const std::wstring& s) {
    std::wstring r=L"\"";size_t slash=0;
    for(wchar_t c:s){if(c==L'\\'){slash++;continue;}if(c==L'"')r.append(slash*2+1,L'\\');else r.append(slash,L'\\');slash=0;r+=c;}
    r.append(slash*2,L'\\');return r+L'"';
}
static bool spawn(const std::wstring& args,PROCESS_INFORMATION& pi) {
    std::wstring command=cmdQuote(exePath())+L" "+args;STARTUPINFOW si{};si.cb=sizeof(si);
    return CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
}
#include "settings_ui.h"
#include "tray_ui.h"
#include "capture_ui.h"
static int run(int argc,wchar_t** argv) {
    std::wstring operation;
    std::wstring captureDirectory;
    if(argc==1)operation=L"--settings";
    for(int i=1;i<argc;i++) {
        std::wstring a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing argument");return std::wstring(argv[i]);};
        if(a==L"--report")report=value();else if(a==L"--panel")panelPath=value();else if(a==L"--left")leftFile=value();else if(a==L"--right")rightFile=value();
        else if(a==L"--width")expectedW=std::stoi(value());else if(a==L"--height")expectedH=std::stoi(value());
        else if(a==L"--parent")parentPid=std::stoul(value());
        else if(a==L"--rounded")rounded=true;else if(a==L"--radius")cornerSize=std::stoi(value());
        else if(a==L"--export-ui"){operation=a;captureDirectory=value();}
        else if(a==L"--debug")debugMode=true;else if(a==L"--demo")demo=true;else if(a==L"--external")allowExternal=true;else if(a==L"--no-private-api")noPin=true;
        else if(a==L"--list"||a==L"--status"||a==L"--stop"||a==L"--supervise"||a==L"--pin-helper"||a==L"--unpin-helper"||a==L"--self-test"||a==L"--validate"||a==L"--settings")operation=a;
        else throw std::runtime_error("Unknown argument");
    }
    cornerCount=rounded?4:2;
    if(operation==L"--export-ui")return exportUI(captureDirectory);
    if(operation==L"--settings")return showSettings();
    if(operation==L"--list") {
        std::string s="{\"physicalContourAvailable\":false,\"panels\":[";bool first=true;
        for(auto& p:panels()){if(!first)s+=',';first=false;s+=panelJSON(p);}output(s+"]}\n");return 0;
    }
    if(operation==L"--pin-helper"||operation==L"--unpin-helper")return pinHelper(operation==L"--unpin-helper");
    if(operation==L"--stop") {HWND ui=FindWindowW(L"GlassCorners.Settings",nullptr);if(ui)PostMessageW(ui,WM_CLOSE,0,0);HANDLE h=OpenEventW(EVENT_MODIFY_STATE,FALSE,objectName(L"Stop").c_str());if(h){SetEvent(h);CloseHandle(h);}return 0;}
    if(operation==L"--status") {
        HANDLE h=OpenFileMappingW(FILE_MAP_READ,FALSE,objectName(L"State").c_str());
        if(!h){output("{\"running\":false}\n");return 0;}
        auto p=static_cast<const State*>(MapViewOfFile(h,FILE_MAP_READ,0,0,sizeof(State)));
        if(!p){CloseHandle(h);throw std::runtime_error("Cannot read state");}State s=*p;UnmapViewOfFile(p);CloseHandle(h);
        output("{\"running\":true,\"pid\":"+std::to_string(s.pid)+",\"supervisor\":"+std::to_string(s.supervisor)+",\"debug\":"+std::to_string(s.debug)+
          ",\"visible\":"+std::to_string(s.visible)+",\"captureExcluded\":"+std::to_string(s.capture)+",\"pinApiAccepted\":"+std::to_string(s.pin)+
          ",\"events\":"+std::to_string(s.events)+",\"renders\":"+std::to_string(s.renders)+",\"visibilityChanges\":"+std::to_string(s.changes)+
          ",\"approximateRounded\":"+std::to_string(s.rounded)+",\"cornerSize\":"+std::to_string(s.cornerSize)+",\"corners\":"+std::to_string(s.rounded?4:2)+
          ",\"enabled\":"+std::to_string(s.enabled)+",\"topEnabled\":"+std::to_string(s.top)+",\"bottomEnabled\":"+std::to_string(s.bottom)+",\"color\":\""+(s.white?"white":"black")+"\",\"layerRepairs\":"+std::to_string(s.layerRepairs)+
          ",\"upPercent\":"+std::to_string(s.up)+",\"downPercent\":"+std::to_string(s.down)+",\"startup\":"+std::to_string(s.startup)+",\"theme\":"+std::to_string(s.theme)+",\"preview\":"+std::to_string(s.preview)+",\"hideFullscreen\":"+std::to_string(s.hideFullscreen)+"}\n");return 0;
    }
    if(operation==L"--self-test") {
        Mask m{3,2,{0,17,255,128,42,0}};auto b=pixels(m,false),d=pixels(m,true);
        if(b[0]!=0x80000000||b[2]!=0||b[5]!=0xff000000||d[0]!=0x80800080||d[5]!=0xffff00ff)throw std::runtime_error("Mask reflection/premultiplication failed");
        if(cmdQuote(L"a b\\")!=L"\"a b\\\\\"")throw std::runtime_error("Command quoting failed");
        wchar_t tempDir[MAX_PATH]{},tempFile[MAX_PATH]{};
        if(!GetTempPathW(MAX_PATH,tempDir)||!GetTempFileNameW(tempDir,L"gct",0,tempFile))throw std::runtime_error("Cannot create settings test file");
        settingsFileOverride=tempFile;Settings test;test.enabled=false;test.top=false;test.white=true;test.up=23;test.down=73;test.startup=false;test.hideFullscreen=true;test.theme=2;
        if(!saveSettings(test))throw std::runtime_error("Settings save failed");
        auto restored=readSettings();
        if(!sameSettings(restored,test))throw std::runtime_error("Settings roundtrip failed");
        test.enabled=true;test.top=true;test.bottom=true;test.white=false;test.up=0;test.down=15;test.startup=true;test.hideFullscreen=false;test.theme=1;normalize(test);
        if(!saveSettings(test))throw std::runtime_error("Second settings save failed");
        restored=readSettings();
        if(!sameSettings(restored,test)||restored.top||!restored.bottom)throw std::runtime_error("Settings replacement/zero-percent failed");
        DeleteFileW(tempFile);settingsFileOverride.clear();settings.white=true;auto white=pixels(m,false);settings.white=false;
        if(white[0]!=0x80808080||white[5]!=0xffffffff)throw std::runtime_error("White premultiplication failed");
        if(radiusFor(25)!=64||radiusFor(50)!=128||radiusFor(100)!=256)throw std::runtime_error("Percentage mapping failed");
        HICON blackIcon=themedIcon(false,32),whiteIcon=themedIcon(true,32);
        if(!blackIcon||!whiteIcon)throw std::runtime_error("User icon resources unavailable");
        DestroyIcon(blackIcon);DestroyIcon(whiteIcon);
        if(!loadPrivateFonts())throw std::runtime_error("SF Pro font resources unavailable");
        HDC fontDC=CreateCompatibleDC(nullptr);HFONT sf=CreateFontW(-24,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,uiFontFamily);
        HGDIOBJ oldFont=SelectObject(fontDC,sf);wchar_t family[128]{};GetTextFaceW(fontDC,128,family);SelectObject(fontDC,oldFont);DeleteObject(sf);DeleteDC(fontDC);
        if(_wcsicmp(family,uiFontFamily))throw std::runtime_error("UI font silently substituted");
        output("{\"maskReflection\":\"pass\",\"alphaPreservation\":\"pass\",\"premultiplication\":\"pass\",\"whitePremultiplication\":\"pass\",\"commandQuoting\":\"pass\",\"settingsRoundtrip\":\"pass\",\"settingsReplacement\":\"pass\",\"zeroPercentDisables\":\"pass\",\"percentageMapping\":\"pass\",\"userIconResources\":\"pass\",\"fontSelection\":\"pass\",\"physicalContourMatch\":\"not_tested\"}\n");return 0;
    }
    settings=readSettings(cornerSize);if(rounded)cornerSize=settings.radius;
    if(demo&&!debugMode)throw std::runtime_error("Synthetic demo requires --debug; no guessed production contour is provided");
    if(cornerSize<(rounded?1:8)||cornerSize>256)throw std::runtime_error("Corner size outside supported range");
    Gdiplus::GdiplusStartupInput gi;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&gi,nullptr)!=Gdiplus::Ok)throw std::runtime_error("GDI+ startup failed");
    if(rounded&&!demo)rebuildRoundedMasks();
    else {masks[0]=demo?fixture(false):loadMask(leftFile);masks[1]=demo?fixture(true):loadMask(rightFile);masks[2]=masks[0];masks[3]=masks[1];}
    if(!demo&&!rounded&&(expectedW<1||expectedH<1))throw std::runtime_error("Exact mask mode requires --width and --height; no resampling is allowed");
    if(!locate())throw std::runtime_error("No uniquely matching built-in panel at the mask resolution. Run --list. Use --debug --demo only for behavior tests");
    Gdiplus::GdiplusShutdown(token);
    if(operation==L"--validate"){output("{\"valid\":true,\"demo\":"+std::string(demo?"true":"false")+",\"panel\":"+panelJSON(selected)+"}\n");return 0;}
    if(operation==L"--supervise") {
        HANDLE mutex=CreateMutexW(nullptr,FALSE,objectName(L"Supervisor").c_str());
        if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
        HANDLE stop=CreateEventW(nullptr,TRUE,FALSE,objectName(L"Stop").c_str());if(!stop)return 1;ResetEvent(stop);
        std::wstring args;
        for(int i=1;i<argc;i++){if(!wcscmp(argv[i],L"--supervise"))continue;args+=cmdQuote(argv[i])+L" ";}
        if(argc==1)args=L"--rounded --external ";
        args+=L"--parent "+std::to_wstring(GetCurrentProcessId());
        // Wait on the child process object, not a periodic health check.
        int crashes=0;DWORD exitCode=0;
        while(WaitForSingleObject(stop,0)!=WAIT_OBJECT_0) {
            PROCESS_INFORMATION pi{};if(!spawn(args,pi)){exitCode=1;break;}CloseHandle(pi.hThread);
            HANDLE handles[]={stop,pi.hProcess};DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
            if(result==WAIT_OBJECT_0) {
                if(WaitForSingleObject(pi.hProcess,3000)==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,0);
                WaitForSingleObject(pi.hProcess,3000);CloseHandle(pi.hProcess);break;
            }
            GetExitCodeProcess(pi.hProcess,&exitCode);CloseHandle(pi.hProcess);
            if(result==WAIT_FAILED){exitCode=1;break;}
            if(++crashes>=5){exitCode=1;break;}
            if(WaitForSingleObject(stop,2000)==WAIT_OBJECT_0)break;
        }
        if(WaitForSingleObject(stop,0)==WAIT_OBJECT_0)exitCode=0;
        CloseHandle(stop);CloseHandle(mutex);return exitCode?1:0;
    }
    HANDLE mutex=CreateMutexW(nullptr,FALSE,objectName(L"Renderer").c_str());
    if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
    stopping=CreateEventW(nullptr,TRUE,FALSE,objectName(L"Stop").c_str());if(!stopping)return 1;
    HANDLE mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(State),objectName(L"State").c_str());
    if(!mapping)throw std::runtime_error("Cannot create status mapping");
    state=static_cast<State*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,sizeof(State)));if(!state)throw std::runtime_error("Cannot map status");
    *state=State{};state->pid=GetCurrentProcessId();state->supervisor=parentPid;state->debug=debugMode;
    state->rounded=rounded;updateStateSettings();
    SetCurrentProcessExplicitAppUserModelID(appId);
    WNDCLASSW wc{};wc.lpfnWndProc=controlProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"GlassCorners.Control";
    if(!RegisterClassW(&wc))throw std::runtime_error("Cannot register controller class");
    controller=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"GlassCorners controller",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    if(!controller)throw std::runtime_error("Cannot create controller");
    shellMessage=RegisterWindowMessageW(L"SHELLHOOK");taskbarMessage=RegisterWindowMessageW(L"TaskbarCreated");RegisterShellHookWindow(controller);
    wc.lpfnWndProc=cornerProc;wc.lpszClassName=L"GlassCorners.Corner";
    if(!RegisterClassW(&wc))throw std::runtime_error("Cannot register corner class");
    bool captureOK=true;
    const wchar_t* titles[]={L"GlassCorners bottom left",L"GlassCorners bottom right",L"GlassCorners top left",L"GlassCorners top right"};
    for(int i=0;i<cornerCount;i++) {
        corners[i]=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,
            titles[i],WS_POPUP,0,0,masks[i].w,masks[i].h,nullptr,nullptr,wc.hInstance,nullptr);
        if(!corners[i])throw std::runtime_error("Cannot create corner window");
        DWORD affinity=0;
        if(!SetWindowDisplayAffinity(corners[i],debugMode?0:0x11)||!GetWindowDisplayAffinity(corners[i],&affinity)||affinity!=(debugMode?0u:0x11u))captureOK=false;
    }
    state->capture=!debugMode&&captureOK;
    // Do not silently expose production overlays if exclusion is unavailable.
    if(!debugMode&&!captureOK)throw std::runtime_error("Capture exclusion unavailable; no production corners shown");
    std::vector<HWINEVENTHOOK> hooks;
    for(DWORD e:{DWORD(EVENT_SYSTEM_FOREGROUND),DWORD(EVENT_OBJECT_LOCATIONCHANGE),DWORD(EVENT_SYSTEM_MOVESIZEEND),DWORD(EVENT_SYSTEM_MINIMIZEEND),DWORD(EVENT_OBJECT_REORDER),DWORD(EVENT_OBJECT_SHOW),DWORD(EVENT_OBJECT_FOCUS)}) {
        HWINEVENTHOOK hook=SetWinEventHook(e,e,nullptr,eventHook,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        if(!hook)throw std::runtime_error("Cannot install fullscreen event hook");
        hooks.push_back(hook);
    }
    refresh(true);
    tray.cbSize=sizeof(tray);tray.hWnd=controller;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=TrayMessage;
    tray.hIcon=brandIcon();lstrcpynW(tray.szTip,L"corner",128);Shell_NotifyIconW(NIM_ADD,&tray);
    PROCESS_INFORMATION helper{};ULONGLONG deadline=0;
    if(!noPin&&spawn(L"--pin-helper",helper)){CloseHandle(helper.hThread);deadline=GetTickCount64()+3000;}
    bool quit=false;
    while(!quit) {
        HANDLE handles[3]={stopping};DWORD count=1,helperIndex=MAXDWORD,previewIndex=MAXDWORD;
        if(helper.hProcess){helperIndex=count;handles[count++]=helper.hProcess;}
        if(previewOwner){previewIndex=count;handles[count++]=previewOwner;}
        DWORD timeout=helper.hProcess?DWORD(std::min<ULONGLONG>(3000,deadline>GetTickCount64()?deadline-GetTickCount64():0)):INFINITE;
        DWORD result=MsgWaitForMultipleObjectsEx(count,handles,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(result==WAIT_OBJECT_0)break;
        if(previewOwner&&result==WAIT_OBJECT_0+previewIndex){applyLiveSettings(readSettings(),false);refresh(false,true);}
        if(helper.hProcess&&(result==WAIT_OBJECT_0+helperIndex||result==WAIT_TIMEOUT)) {
            DWORD code=1;
            if(result==WAIT_TIMEOUT)TerminateProcess(helper.hProcess,22);else GetExitCodeProcess(helper.hProcess,&code);
            state->pin=code==0?1:-1;CloseHandle(helper.hProcess);helper.hProcess=nullptr;
        }
        if(result==WAIT_FAILED)throw std::runtime_error("Message wait failed");
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){quit=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);
        }
    }
    if(helper.hProcess){TerminateProcess(helper.hProcess,0);CloseHandle(helper.hProcess);}
    if(previewOwner)CloseHandle(previewOwner);
    for(auto h:hooks)UnhookWinEvent(h);
    Shell_NotifyIconW(NIM_DELETE,&tray);DestroyIcon(tray.hIcon);
    for(HWND c:corners)DestroyWindow(c);
    DeregisterShellHookWindow(controller);DestroyWindow(controller);
    UnmapViewOfFile(state);CloseHandle(mapping);CloseHandle(stopping);CloseHandle(mutex);return 0;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    try{int result=run(argc,argv);LocalFree(argv);return result;}
    catch(const std::exception& e){try{output("{\"error\":"+quoted(std::wstring(e.what(),e.what()+strlen(e.what())))+"}\n");}catch(...){}LocalFree(argv);return 1;}
}
