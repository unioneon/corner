// Capture the actual native controls via WM_PRINT, without touching user preferences.
static void saveWindowPNG(HWND window,const std::wstring& path,const std::vector<BYTE>* alpha=nullptr) {
    RECT bounds{};GetClientRect(window,&bounds);int width=bounds.right,height=bounds.bottom;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* raw=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!dc||!bitmap||!raw)throw std::runtime_error("Cannot allocate screenshot");
    ZeroMemory(raw,size_t(width)*height*4);HGDIOBJ old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    Gdiplus::Bitmap image(width,height,PixelFormat32bppPARGB);Gdiplus::Rect rect(0,0,width,height);Gdiplus::BitmapData data{};
    if(image.LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok)throw std::runtime_error("Cannot encode screenshot");
    HRGN region=CreateRectRgn(0,0,0,0);bool hasRegion=GetWindowRgn(window,region)!=ERROR;
    for(int y=0;y<height;y++) {
        BYTE* out=static_cast<BYTE*>(data.Scan0)+y*data.Stride;const BYTE* source=static_cast<BYTE*>(raw)+size_t(y)*width*4;
        for(int x=0;x<width;x++) {
            BYTE a=alpha&&alpha->size()==size_t(width)*height?(*alpha)[size_t(y)*width+x]:BYTE(!hasRegion||PtInRegion(region,x,y)?255:0);
            for(int c=0;c<3;c++)out[x*4+c]=BYTE((unsigned(source[x*4+c])*a+127)/255);
            out[x*4+3]=a;
        }
    }
    DeleteObject(region);image.UnlockBits(&data);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> encoders(size);Gdiplus::GetImageEncoders(count,size,reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data()));
    for(UINT i=0;i<count;i++){auto& encoder=reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data())[i];if(!wcscmp(encoder.MimeType,L"image/png")){if(image.Save(path.c_str(),&encoder.Clsid,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Cannot save screenshot PNG");return;}}
    throw std::runtime_error("PNG encoder unavailable");
}
static int exportUI(const std::wstring& directory) {
    CreateDirectoryW(directory.c_str(),nullptr);wchar_t tempDir[MAX_PATH]{},temp[MAX_PATH]{};GetTempPathW(MAX_PATH,tempDir);GetTempFileNameW(tempDir,L"crn",0,temp);settingsFileOverride=temp;
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Capture graphics unavailable");loadPrivateFonts();
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
    Settings sample;sample.up=50;sample.down=50;sample.top=true;sample.bottom=false;sample.theme=1;normalize(sample);saveSettings(sample);
    WNDCLASSW cls{};cls.lpfnWndProc=CornerUI::proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"GlassCorners.Settings";RegisterClassW(&cls);
    UINT dpi=GetDpiForSystem();HWND window=CreateWindowExW(0,cls.lpszClassName,L"corner capture",WS_POPUP|WS_CLIPCHILDREN,0,0,MulDiv(920,dpi,96),MulDiv(159,dpi,96),nullptr,nullptr,cls.hInstance,nullptr);
    if(!window)throw std::runtime_error("Cannot create capture window");
    size_t semitransparent=0;for(BYTE alpha:CornerUI::edgeAlpha)if(alpha>0&&alpha<255)semitransparent++;
    saveWindowPNG(window,directory+L"\\settings-light.png",&CornerUI::edgeAlpha);
    sample.theme=2;CornerUI::draft=CornerUI::applied=sample;CornerUI::updated();saveWindowPNG(window,directory+L"\\settings-dark.png",&CornerUI::edgeAlpha);
    DestroyWindow(window);if(CornerUI::logoIcon){DestroyIcon(CornerUI::logoIcon);CornerUI::logoIcon=nullptr;}if(CornerUI::faceBrush){DeleteObject(CornerUI::faceBrush);CornerUI::faceBrush=nullptr;}
    DeleteFileW(temp);settingsFileOverride.clear();
    output("{\"nativeUiCapture\":true,\"font\":"+quoted(uiFontFamily)+",\"antialiasedEdgePixels\":"+std::to_string(semitransparent)+"}\n");
    Gdiplus::GdiplusShutdown(token);return semitransparent?0:1;
}
