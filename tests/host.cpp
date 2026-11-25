#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
static bool full=false;
static RECT normal{200,150,1000,750};
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_KEYDOWN&&(a==VK_F11||a==VK_ESCAPE)) {
        full=a==VK_ESCAPE?false:!full;
        SetWindowLongPtrW(w,GWL_STYLE,full?WS_POPUP:(WS_OVERLAPPEDWINDOW|WS_VISIBLE));
        MONITORINFO info{sizeof(info),{},{},0};GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTOPRIMARY),&info);
        RECT r=full?info.rcMonitor:normal;
        SetWindowPos(w,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
        InvalidateRect(w,nullptr,TRUE);return 0;
    }
    if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetTextColor(dc,RGB(40,40,40));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"GlassCorners test host\nF11: fullscreen / restore\nEscape: restore\nClose this window after testing",-1,&r,DT_CENTER|DT_VCENTER|DT_WORDBREAK);EndPaint(w,&ps);return 0;}
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=instance;c.lpszClassName=L"GlassCorners.TestHost";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);
    HWND w=CreateWindowW(c.lpszClassName,L"GlassCorners test host",WS_OVERLAPPEDWINDOW|WS_VISIBLE,normal.left,normal.top,normal.right-normal.left,normal.bottom-normal.top,nullptr,nullptr,instance,nullptr);
    ShowWindow(w,SW_SHOW);SetForegroundWindow(w);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
