namespace CornerTray {
    static HWND popup=nullptr;static UINT choice=0,dpi=96;static bool dark=false,active=false;
    static HICON icon=nullptr;static HBRUSH background=nullptr;
    static float scale(){return dpi/96.0f;}
    static int px(float value){return int(std::lround(value*scale()));}
    static Gdiplus::Color face(){return dark?Gdiplus::Color(255,28,28,30):Gdiplus::Color(255,255,255,255);}
    static void paintButton(DRAWITEMSTRUCT* draw) {
        Gdiplus::Graphics g(draw->hDC);g.Clear(face());g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.ScaleTransform(scale(),scale());
        float width=(draw->rcItem.right-draw->rcItem.left)/scale(),height=(draw->rcItem.bottom-draw->rcItem.top)/scale();
        if(draw->itemState&(ODS_SELECTED|ODS_FOCUS)) {Gdiplus::GraphicsPath path;CornerUI::roundPath(path,0,0,width,height,12);Gdiplus::SolidBrush hover(dark?Gdiplus::Color(255,43,43,47):Gdiplus::Color(255,246,246,247));g.FillPath(&hover,&path);}
        const wchar_t* caption=draw->CtlID==1?L"Settings":(draw->CtlID==4?L"Game":(draw->CtlID==2?(settings.enabled&&(settings.top||settings.bottom)?L"Disable":L"Enable"):L"Exit"));
        float inset=draw->CtlID==4&&settings.game?28.0f:8.0f;
        if(draw->CtlID==4&&settings.game){Gdiplus::SolidBrush dot(Gdiplus::Color(255,25,110,254));g.FillEllipse(&dot,9.0f,height/2-4.0f,8.0f,8.0f);}
        drawSF(g,caption,{inset,0,width-inset-8,height},30,Gdiplus::Color(255,179,179,179),false,false,scale());
    }
    static void paintSurface(HDC dc) {
        Gdiplus::Graphics g(dc);g.Clear(face());g.ScaleTransform(scale(),scale());
        DrawIconEx(dc,px(30),px(27),icon,px(26),px(26),0,nullptr,DI_NORMAL);
        drawSF(g,L"corner",{68,22,160,37},25,dark?Gdiplus::Color(255,255,255,255):Gdiplus::Color(255,0,0,0),true,false,scale());
    }
    static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM a,LPARAM b) {
        if(msg==WM_DRAWITEM){paintButton(reinterpret_cast<DRAWITEMSTRUCT*>(b));return TRUE;}
        if(msg==WM_COMMAND&&HIWORD(a)==BN_CLICKED){choice=LOWORD(a);DestroyWindow(w);return 0;}
        if(msg==WM_ACTIVATE&&LOWORD(a)==WA_INACTIVE&&IsWindowVisible(w)){DestroyWindow(w);return 0;}
        if(msg==WM_ERASEBKGND)return 1;
        if(msg==WM_PAINT) {
            PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);paintSurface(dc);EndPaint(w,&ps);return 0;
        }
        if(msg==WM_PRINTCLIENT){paintSurface(reinterpret_cast<HDC>(a));return 0;}
        if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
        return DefWindowProcW(w,msg,a,b);
    }
    static void createButtons(HWND parent) {
        const int ids[]={1,4,2,3};
        for(int row=0;row<4;row++) {
            int id=ids[row];
            CreateWindowExW(0,L"BUTTON",id==1?L"Settings":(id==4?L"Game":(id==2?(settings.enabled?L"Disable":L"Enable"):L"Exit")),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,px(22),px(73+row*52),px(220),px(48),parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        }
    }
}
static UINT showTrayPopup(HWND owner) {
    using namespace CornerTray;
    if(active)return 0;
    active=true;choice=0;dark=darkTheme(settings);
    if(!loadPrivateFonts()){active=false;return 0;}
    ULONG_PTR token=0;Gdiplus::GdiplusStartupInput startup;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok){active=false;return 0;}
    dpi=GetDpiForWindow(owner);background=CreateSolidBrush(dark?RGB(28,28,30):RGB(255,255,255));icon=themedIcon(dark,px(26));
    WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=proc;c.lpszClassName=L"corner.TrayMenu";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassW(&c);
    POINT point{};GetCursorPos(&point);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTOPRIMARY),&monitor);
    int width=px(260),height=px(298);
    int x=std::clamp(int(point.x)-width+px(16),int(monitor.rcWork.left+px(8)),int(monitor.rcWork.right-width-px(8)));
    int y=std::max(int(monitor.rcWork.top+px(8)),std::min(int(point.y)-height-px(10),int(monitor.rcWork.bottom-height-px(8))));
    popup=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,c.lpszClassName,L"corner",WS_POPUP|WS_CLIPCHILDREN,x,y,width,height,owner,nullptr,c.hInstance,nullptr);
    if(popup) {
        SetWindowRgn(popup,CreateRoundRectRgn(0,0,width+1,height+1,px(80),px(80)),TRUE);
        createButtons(popup);
        ShowWindow(popup,SW_SHOW);SetForegroundWindow(popup);SetFocus(GetDlgItem(popup,1));
        bool stop=false;
        while(IsWindow(popup)&&!stop) {
            DWORD result=MsgWaitForMultipleObjectsEx(1,&stopping,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_OBJECT_0||result==WAIT_FAILED){DestroyWindow(popup);break;}
            MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));stop=true;break;}
                if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){DestroyWindow(popup);break;}
                if(!IsDialogMessageW(popup,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
                if(!IsWindow(popup))break;
            }
        }
        if(IsWindow(popup))DestroyWindow(popup);
    }
    if(icon)DestroyIcon(icon);
    if(background)DeleteObject(background);
    Gdiplus::GdiplusShutdown(token);active=false;return choice;
}
