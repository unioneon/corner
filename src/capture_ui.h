// Capture the actual native controls via WM_PRINT, without touching user preferences.
static void saveWindowPNG(HWND window,const std::wstring& path,const SmoothFrame::Surface& frame) {
    int width=frame.width,height=frame.height,pad=frame.padding;
    int outputWidth=width+pad*2,outputHeight=height+pad*2;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!bitmap||!raw)throw std::runtime_error("Cannot allocate screenshot");
    ZeroMemory(raw,size_t(width)*height*4);HGDIOBJ old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    Gdiplus::Bitmap image(outputWidth,outputHeight,PixelFormat32bppPARGB);Gdiplus::Rect rect(0,0,outputWidth,outputHeight);Gdiplus::BitmapData data{};
    if(image.LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok)throw std::runtime_error("Cannot encode screenshot");
    for(int y=0;y<outputHeight;y++) {
        DWORD* out=reinterpret_cast<DWORD*>(static_cast<BYTE*>(data.Scan0)+y*data.Stride);
        for(int x=0;x<outputWidth;x++) {
            int cx=x-pad,cy=y-pad;
            if(cx>=0&&cx<width&&cy>=0&&cy<height&&frame.alpha[size_t(cy)*width+cx]==255)
                out[x]=static_cast<DWORD*>(raw)[size_t(cy)*width+cx]|0xff000000;
            else out[x]=frame.pixels[size_t(y)*outputWidth+x];
        }
    }
    image.UnlockBits(&data);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> encoders(size);Gdiplus::GetImageEncoders(count,size,reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data()));
    for(UINT i=0;i<count;i++){auto& encoder=reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data())[i];if(!wcscmp(encoder.MimeType,L"image/png")){if(image.Save(path.c_str(),&encoder.Clsid,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Cannot save screenshot PNG");return;}}
    throw std::runtime_error("PNG encoder unavailable");
}
static size_t verifyFrame(HWND window,const SmoothFrame::Surface& frame) {
    if(!frame.backdrop||frame.alpha.empty())throw std::runtime_error("Missing alpha frame");
    size_t partial=0;HRGN region=CreateRectRgn(0,0,0,0);GetWindowRgn(window,region);
    bool regionMatches=true;
    for(int y=0;y<frame.height;y++)for(int x=0;x<frame.width;x++) {
        BYTE a=frame.alpha[size_t(y)*frame.width+x];if(a>0&&a<255)partial++;
        if(bool(PtInRegion(region,x,y))!=(a==255))regionMatches=false;
    }
    DeleteObject(region);
    if(!partial||!regionMatches)throw std::runtime_error("Alpha frame and opaque input region disagree");
    bool outsideShadow=false;int fullWidth=frame.width+frame.padding*2,fullHeight=frame.height+frame.padding*2;
    for(int y=0;y<fullHeight;y++)for(int x=0;x<fullWidth;x++)
        if((x<frame.padding||x>=frame.padding+frame.width||y<frame.padding||y>=frame.padding+frame.height)&&(frame.pixels[size_t(y)*fullWidth+x]>>24))outsideShadow=true;
    if(outsideShadow!=frame.shadowEnabled)throw std::runtime_error("Shadow coverage is incorrect");
    SetWindowPos(window,nullptr,-30000,-30000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    ShowWindow(window,SW_SHOWNOACTIVATE);
    if(!IsWindowVisible(frame.backdrop))throw std::runtime_error("Alpha frame did not show with its window");
    using WindowBand=BOOL(WINAPI*)(HWND,DWORD*);
    FARPROC address=GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetWindowBand");
    WindowBand windowBand=nullptr;static_assert(sizeof(windowBand)==sizeof(address));CopyMemory(&windowBand,&address,sizeof(windowBand));
    DWORD contentBand=0,frameBand=0;
    if(windowBand&&windowBand(window,&contentBand)&&windowBand(frame.backdrop,&frameBand)&&contentBand!=frameBand)
        throw std::runtime_error("Alpha frame and controls are in different window bands");
    SetWindowPos(window,nullptr,-29000,-29000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    RECT face{},edge{};GetWindowRect(window,&face);GetWindowRect(frame.backdrop,&edge);
    if(edge.left!=face.left-frame.padding||edge.top!=face.top-frame.padding)throw std::runtime_error("Alpha frame did not follow its window");
    ShowWindow(window,SW_HIDE);
    if(IsWindowVisible(frame.backdrop))throw std::runtime_error("Alpha frame remained after hiding window");
    return partial;
}
static int exportUI(const std::wstring& directory) {
    CreateDirectoryW(directory.c_str(),nullptr);wchar_t tempDir[MAX_PATH]{},temp[MAX_PATH]{};GetTempPathW(MAX_PATH,tempDir);GetTempFileNameW(tempDir,L"crn",0,temp);settingsFileOverride=temp;
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Capture graphics unavailable");loadPrivateFonts();
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
    Settings sample;sample.up=50;sample.down=50;sample.top=true;sample.bottom=false;sample.theme=1;normalize(sample);saveSettings(sample);
    WNDCLASSW cls{};cls.lpfnWndProc=CornerUI::proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"GlassCorners.Settings";RegisterClassW(&cls);
    UINT dpi=GetDpiForSystem();HWND window=CreateWindowExW(0,cls.lpszClassName,L"corner capture",WS_POPUP|WS_CLIPCHILDREN,0,0,MulDiv(920,dpi,96),MulDiv(159,dpi,96),nullptr,nullptr,cls.hInstance,nullptr);
    if(!window)throw std::runtime_error("Cannot create capture window");
    size_t semitransparent=0;for(BYTE alpha:CornerUI::frame.alpha)if(alpha>0&&alpha<255)semitransparent++;
    saveWindowPNG(window,directory+L"\\settings-light.png",CornerUI::frame);
    verifyFrame(window,CornerUI::frame);
    sample.theme=2;CornerUI::draft=CornerUI::applied=sample;CornerUI::updated();saveWindowPNG(window,directory+L"\\settings-dark.png",CornerUI::frame);
    verifyFrame(window,CornerUI::frame);
    for(UINT testDpi:{96u,120u,144u,192u}) {
        RECT r{-30000,-30000,-30000+MulDiv(920,testDpi,96),-30000+MulDiv(159,testDpi,96)};
        SendMessageW(window,WM_DPICHANGED,MAKELONG(testDpi,testDpi),reinterpret_cast<LPARAM>(&r));verifyFrame(window,CornerUI::frame);
    }
    DestroyWindow(window);if(CornerUI::logoIcon){DestroyIcon(CornerUI::logoIcon);CornerUI::logoIcon=nullptr;}if(CornerUI::faceBrush){DeleteObject(CornerUI::faceBrush);CornerUI::faceBrush=nullptr;}
    // Export the same native tray controls used at runtime, without opening a
    // modal popup or changing the user's saved Game preference.
    Settings previous=settings;settings.game=true;settings.enabled=true;
    WNDCLASSW trayClass{};trayClass.lpfnWndProc=CornerTray::proc;trayClass.hInstance=GetModuleHandleW(nullptr);trayClass.lpszClassName=L"corner.TrayMenu";RegisterClassW(&trayClass);
    CornerTray::dpi=dpi;
    size_t trayPartial=0;
    for(bool dark:{false,true}) {
        CornerTray::dark=dark;CornerTray::icon=themedIcon(dark,CornerTray::px(26));
        int width=CornerTray::px(260),height=CornerTray::px(298);
        HWND menu=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,trayClass.lpszClassName,L"corner tray capture",WS_POPUP|WS_CLIPCHILDREN,0,0,width,height,nullptr,nullptr,trayClass.hInstance,nullptr);
        if(!menu)throw std::runtime_error("Cannot create tray capture");
        CornerTray::shape(menu);
        CornerTray::createButtons(menu);saveWindowPNG(menu,directory+(dark?L"\\tray-game-dark.png":L"\\tray-game-light.png"),CornerTray::frame);
        trayPartial+=verifyFrame(menu,CornerTray::frame);
        for(UINT testDpi:{96u,120u,144u,192u}) {
            CornerTray::dpi=testDpi;
            SetWindowPos(menu,nullptr,0,0,CornerTray::px(260),CornerTray::px(298),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            CornerTray::shape(menu);verifyFrame(menu,CornerTray::frame);
        }
        CornerTray::dpi=dpi;
        DestroyWindow(menu);DestroyIcon(CornerTray::icon);CornerTray::icon=nullptr;
    }
    settings=previous;DeleteFileW(temp);settingsFileOverride.clear();
    output("{\"nativeUiCapture\":true,\"font\":"+quoted(uiFontFamily)+",\"antialiasedEdgePixels\":"+std::to_string(semitransparent)+",\"trayAntialiasedEdgePixels\":"+std::to_string(trayPartial)+",\"frameLifecycle\":true,\"dpiChecks\":4}\n");
    Gdiplus::GdiplusShutdown(token);return semitransparent?0:1;
}
