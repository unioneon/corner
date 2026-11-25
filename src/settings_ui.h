// Compact native UI drawn from the user's 736 x 127 reference.
static HICON themedIcon(bool dark,int size) {
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(dark?102:101),IMAGE_ICON,size,size,0));
}
static HICON brandIcon(){return themedIcon(!registryLight(false),GetSystemMetrics(SM_CXSMICON));}
static void openSettings(){PROCESS_INFORMATION pi{};if(spawn(L"--settings",pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}}
namespace CornerUI {
    enum {Up=101,Down=102,Startup=103,UpSlider=104,DownSlider=105,Apply=106,OK=107,Logo=108};
    static HWND window=nullptr;static UINT dpi=96;static float zoom=1.25f;static Settings draft,applied;
    static ULONG_PTR graphicsToken=0;static HBRUSH faceBrush=nullptr;static HICON logoIcon=nullptr;static bool lastDark=false;
    static bool previewSent=false;
    static Settings displayed;
    static bool displayedValid=false;
    static HWND backdropWindow=nullptr;
    static std::vector<BYTE> edgeAlpha;
    static int edgeWidth=0,edgeHeight=0;
    template<class Painter> static void buffered(HDC target,int width,int height,Painter paint,const RECT* dirty=nullptr) {
        HDC buffer=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,width,height);
        if(!buffer||!bitmap){if(bitmap)DeleteObject(bitmap);if(buffer)DeleteDC(buffer);paint(target);return;}
        HGDIOBJ old=SelectObject(buffer,bitmap);paint(buffer);
        RECT r=dirty?*dirty:RECT{0,0,width,height};
        BitBlt(target,r.left,r.top,r.right-r.left,r.bottom-r.top,buffer,r.left,r.top,SRCCOPY);
        SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
    }
    static float factor(){return zoom*dpi/96.0f;}
    static int px(float value){return int(std::lround(value*factor()));}
    static bool dirty(){return !sameSettings(draft,applied);}
    static bool isDark(){return darkTheme(draft);}
    static Gdiplus::Color face(){return isDark()?Gdiplus::Color(255,28,28,30):Gdiplus::Color(255,255,255,255);}
    static Gdiplus::Color label(){return isDark()?Gdiplus::Color(255,166,166,174):Gdiplus::Color(255,179,179,179);}
    static Gdiplus::Color pale(){return isDark()?Gdiplus::Color(255,46,46,49):Gdiplus::Color(255,247,247,247);}
    static Gdiplus::Color blue(){return Gdiplus::Color(255,25,110,254);}
    static HWND item(int id){return GetDlgItem(window,id);}
    static void text(Gdiplus::Graphics& g,const wchar_t* value,Gdiplus::RectF r,float size,Gdiplus::Color color,bool bold=false,bool center=false) {
        drawSF(g,value,r,size,color,bold,center,factor());
    }
    static void roundPath(Gdiplus::GraphicsPath& p,float x,float y,float w,float h,float radius) {
        float d=radius*2;p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();
    }
    static void outerPath(Gdiplus::GraphicsPath& p) {
        // The same continuous-corner contour as the supplied menu SVG.
        p.AddBezier(0.0f,63.5f,0.0f,91.5038f,0.0f,105.506f,7.81032f,114.877f);
        p.AddBezier(7.81032f,114.877f,9.11445f,116.442f,10.5578f,117.886f,12.1226f,119.19f);
        p.AddBezier(12.1226f,119.19f,21.4943f,127.0f,35.4962f,127.0f,63.5f,127.0f);
        p.AddLine(63.5f,127.0f,672.5f,127.0f);
        p.AddBezier(672.5f,127.0f,700.504f,127.0f,714.506f,127.0f,723.877f,119.19f);
        p.AddBezier(723.877f,119.19f,725.442f,117.886f,726.886f,116.442f,728.19f,114.877f);
        p.AddBezier(728.19f,114.877f,736.0f,105.506f,736.0f,91.5038f,736.0f,63.5f);
        p.AddBezier(736.0f,63.5f,736.0f,35.4962f,736.0f,21.4943f,728.19f,12.1226f);
        p.AddBezier(728.19f,12.1226f,726.886f,10.5578f,725.442f,9.11445f,723.877f,7.81032f);
        p.AddBezier(723.877f,7.81032f,714.506f,0.0f,700.504f,0.0f,672.5f,0.0f);
        p.AddLine(672.5f,0.0f,63.5f,0.0f);
        p.AddBezier(63.5f,0.0f,35.4962f,0.0f,21.4943f,0.0f,12.1226f,7.81032f);
        p.AddBezier(12.1226f,7.81032f,10.5578f,9.11445f,9.11445f,10.5578f,7.81032f,12.1226f);
        p.AddBezier(7.81032f,12.1226f,0.0f,21.4943f,0.0f,35.4962f,0.0f,63.5f);p.CloseFigure();
    }
    static LRESULT CALLBACK backdropProc(HWND w,UINT msg,WPARAM a,LPARAM b) {
        if(msg==WM_NCHITTEST)return HTTRANSPARENT;
        if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        return DefWindowProcW(w,msg,a,b);
    }
    static void moveBackdrop() {
        if(!backdropWindow)return;
        RECT r{};GetWindowRect(window,&r);
        SetWindowPos(backdropWindow,nullptr,r.left,r.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    static void shape() {
        RECT bounds{},position{};GetClientRect(window,&bounds);GetWindowRect(window,&position);
        int width=bounds.right,height=bounds.bottom;if(width<1||height<1)return;
        if(!backdropWindow) {
            WNDCLASSW c{};c.lpfnWndProc=backdropProc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"corner.SmoothBackdrop";RegisterClassW(&c);
            backdropWindow=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,c.lpszClassName,L"",WS_POPUP,position.left,position.top,width,height,nullptr,nullptr,c.hInstance,nullptr);
            if(!backdropWindow)return;
            // The interactive window stays above its click-through alpha backdrop.
            SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(backdropWindow));
        }
        Gdiplus::Bitmap supersampled(width*4,height*4,PixelFormat32bppPARGB);
        {Gdiplus::Graphics g(&supersampled);g.Clear(Gdiplus::Color(0,0,0,0));g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.ScaleTransform(factor()*4,factor()*4);Gdiplus::GraphicsPath path;outerPath(path);Gdiplus::SolidBrush brush(face());g.FillPath(&brush,&path);}
        Gdiplus::Bitmap image(width,height,PixelFormat32bppPARGB);
        {Gdiplus::Graphics g(&image);g.Clear(Gdiplus::Color(0,0,0,0));g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);g.DrawImage(&supersampled,0,0,width,height);}
        Gdiplus::Rect rect(0,0,width,height);Gdiplus::BitmapData pixels{};
        if(image.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&pixels)!=Gdiplus::Ok)return;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        HDC dc=CreateCompatibleDC(nullptr);void* raw=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0);
        if(!dc||!bitmap||!raw){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);image.UnlockBits(&pixels);return;}
        edgeWidth=width;edgeHeight=height;edgeAlpha.resize(width*height);std::vector<RECT> spans;
        for(int y=0;y<height;y++) {
            const BYTE* row=static_cast<const BYTE*>(pixels.Scan0)+y*pixels.Stride;CopyMemory(static_cast<BYTE*>(raw)+y*width*4,row,width*4);
            int first=width,last=-1;
            for(int x=0;x<width;x++){BYTE alpha=row[x*4+3];edgeAlpha[y*width+x]=alpha;if(alpha==255){first=std::min(first,x);last=x;}}
            if(last>=first)spans.push_back({first,y,last+1,y+1});
        }
        image.UnlockBits(&pixels);HGDIOBJ old=SelectObject(dc,bitmap);POINT dst{position.left,position.top},src{};SIZE size{width,height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        BOOL ok=UpdateLayeredWindow(backdropWindow,nullptr,&dst,&size,dc,&src,0,&blend,ULW_ALPHA);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
        if(ok&&!spans.empty()) {
            std::vector<BYTE> data(sizeof(RGNDATAHEADER)+spans.size()*sizeof(RECT));auto region=reinterpret_cast<RGNDATA*>(data.data());region->rdh={sizeof(RGNDATAHEADER),RDH_RECTANGLES,DWORD(spans.size()),DWORD(spans.size()*sizeof(RECT)),{0,0,width,height}};
            CopyMemory(region->Buffer,spans.data(),spans.size()*sizeof(RECT));HRGN native=ExtCreateRegion(nullptr,DWORD(data.size()),region);if(native)SetWindowRgn(window,native,TRUE);
            if(IsWindowVisible(window))ShowWindow(backdropWindow,SW_SHOWNOACTIVATE);
        }
    }
    static bool refreshTheme() {
        bool dark=isDark();if(logoIcon&&dark==lastDark)return false;lastDark=dark;
        if(faceBrush)DeleteObject(faceBrush);
        faceBrush=CreateSolidBrush(dark?RGB(28,28,30):RGB(255,255,255));
        if(logoIcon)DestroyIcon(logoIcon);
        logoIcon=themedIcon(dark,px(44));
        SendMessageW(window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(logoIcon));return true;
    }
    static void previewDraft() {
        HWND renderer=FindWindowW(L"GlassCorners.Control",L"GlassCorners controller");if(!renderer)return;
        COPYDATASTRUCT copy{};copy.dwData=0x434f524e;copy.cbData=sizeof(Settings);copy.lpData=&draft;DWORD_PTR result=0;
        if(SendMessageTimeoutW(renderer,WM_COPYDATA,reinterpret_cast<WPARAM>(window),reinterpret_cast<LPARAM>(&copy),SMTO_ABORTIFHUNG,1000,&result)&&result)previewSent=true;
    }
    static void rollbackPreview() {
        if(!previewSent)return;
        HWND renderer=FindWindowW(L"GlassCorners.Control",L"GlassCorners controller");DWORD_PTR result=0;
        if(renderer)SendMessageTimeoutW(renderer,ReloadSettings,0,0,SMTO_ABORTIFHUNG,2000,&result);
        previewSent=false;
    }
    static void updated(bool preview=false) {
        normalize(draft);
        bool effectChanged=!displayedValid||displayed.enabled!=draft.enabled||displayed.top!=draft.top||displayed.bottom!=draft.bottom||displayed.up!=draft.up||displayed.down!=draft.down||displayed.white!=draft.white;
        bool applyEnabled=dirty();if(bool(IsWindowEnabled(item(Apply)))!=applyEnabled)EnableWindow(item(Apply),applyEnabled);
        bool themeChanged=refreshTheme();
        auto invalidate=[](int id){InvalidateRect(item(id),nullptr,FALSE);};
        auto percent=[](bool up){RECT r{px(402),px(up?23:65),px(467),px(up?59:101)};InvalidateRect(window,&r,FALSE);};
        if(themeChanged&&backdropWindow)shape();
        if(!displayedValid||themeChanged) {
            InvalidateRect(window,nullptr,FALSE);
            for(int id:{Up,Down,Startup,Apply,OK,Logo,UpSlider,DownSlider})invalidate(id);
        } else {
            if(displayed.top!=draft.top)invalidate(Up);
            if(displayed.bottom!=draft.bottom)invalidate(Down);
            if(displayed.startup!=draft.startup)invalidate(Startup);
            if(displayed.up!=draft.up){percent(true);invalidate(UpSlider);}
            if(displayed.down!=draft.down){percent(false);invalidate(DownSlider);}
        }
        displayed=draft;displayedValid=true;
        if(preview&&effectChanged)previewDraft();
    }
    static bool commit() {
        normalize(draft);Settings before=readSettings();bool startupBefore=startupEnabled();
        if(draft.startup!=startupBefore) {
            HRESULT hr=setStartup(draft.startup);
            if(FAILED(hr)){MessageBoxW(window,L"Could not change Start Windows. Your changes have not been applied.",L"corner",MB_OK|MB_ICONERROR);return false;}
        }
        if(!saveSettings(draft)) {
            if(draft.startup!=startupBefore)setStartup(startupBefore);
            MessageBoxW(window,L"Could not save settings. Your changes have not been applied.",L"corner",MB_OK|MB_ICONERROR);return false;
        }
        DWORD_PTR result=0;HWND renderer=FindWindowW(L"GlassCorners.Control",L"GlassCorners controller");
        if(!renderer||!SendMessageTimeoutW(renderer,ReloadSettings,0,0,SMTO_ABORTIFHUNG,3000,&result)||!result) {
            saveSettings(before);if(draft.startup!=startupBefore)setStartup(startupBefore);
            MessageBoxW(window,L"The background renderer did not respond. Please try Apply again.",L"corner",MB_OK|MB_ICONERROR);return false;
        }
        previewSent=false;applied=draft;updated();return true;
    }
    static void appearanceMenu(POINT point) {
        HMENU menu=CreatePopupMenu();HMENU colors=CreatePopupMenu(),themes=CreatePopupMenu();
        AppendMenuW(colors,MF_STRING|(!draft.white?MF_CHECKED:0),10,L"Black");AppendMenuW(colors,MF_STRING|(draft.white?MF_CHECKED:0),11,L"White");
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(colors),L"Corner color");
        const wchar_t* names[]={L"System",L"Light",L"Dark"};for(int i=0;i<3;i++)AppendMenuW(themes,MF_STRING|(draft.theme==i?MF_CHECKED:0),20+i,names[i]);
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(themes),L"Appearance");
        UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);DestroyMenu(menu);
        if(choice==10||choice==11)draft.white=choice==11;
        if(choice>=20&&choice<=22)draft.theme=choice-20;
        updated(true);
    }
    static LRESULT CALLBACK noErase(HWND w,UINT msg,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR) {
        if(msg==WM_ERASEBKGND)return 1;
        if(msg==WM_WINDOWPOSCHANGED){moveBackdrop();}
        if(msg==WM_SHOWWINDOW&&backdropWindow){ShowWindow(backdropWindow,a?SW_SHOWNOACTIVATE:SW_HIDE);}
        return DefSubclassProc(w,msg,a,b);
    }
    static HWND add(const wchar_t* cls,const wchar_t* title,int id,DWORD style,float x,float y,float w,float h) {
        HWND result=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS|style,px(x),px(y),px(w),px(h),window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        SetWindowSubclass(result,noErase,1,0);return result;
    }
    static void controls() {
        displayedValid=false;
        add(L"BUTTON",L"corner: color and appearance",Logo,BS_OWNERDRAW,20,28,64,77);
        add(L"BUTTON",L"Up",Up,BS_OWNERDRAW,167,21,113,38);
        add(L"BUTTON",L"Down",Down,BS_OWNERDRAW,167,63,117,38);
        add(L"BUTTON",L"Start Windows",Startup,BS_OWNERDRAW,500,21,232,38);
        for(int i=0;i<2;i++) {
            HWND slider=add(TRACKBAR_CLASSW,i?L"Down percentage":L"Up percentage",i?DownSlider:UpSlider,TBS_HORZ|TBS_NOTICKS|TBS_FIXEDLENGTH,284,i?71:29,112,29);
            SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELONG(0,100));SendMessageW(slider,TBM_SETPAGESIZE,0,10);SendMessageW(slider,TBM_SETTHUMBLENGTH,px(11),0);
            SendMessageW(slider,TBM_SETPOS,TRUE,i?draft.down:draft.up);
        }
        add(L"BUTTON",L"Apply",Apply,BS_OWNERDRAW,506,74,89,37);
        add(L"BUTTON",L"OK",OK,BS_OWNERDRAW,609,74,89,37);
        HWND tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tool.hwnd=window;tool.uId=reinterpret_cast<UINT_PTR>(item(Logo));tool.lpszText=const_cast<wchar_t*>(L"Corner color · Light / Dark theme");SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
        updated();
    }
    static void paintButton(DRAWITEMSTRUCT* draw,HDC dc) {
        Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.Clear(face());g.ScaleTransform(factor(),factor());float w=(draw->rcItem.right-draw->rcItem.left)/factor(),h=(draw->rcItem.bottom-draw->rcItem.top)/factor();
        int id=int(draw->CtlID);bool disabled=draw->itemState&ODS_DISABLED,pressed=draw->itemState&ODS_SELECTED;
        if(id==Logo) {
            auto state=g.Save();g.ResetTransform();DrawIconEx(dc,px(8),px(9),logoIcon,px(44),px(44),0,nullptr,DI_NORMAL);g.Restore(state);
            text(g,L"corner",{0,55,w,20},12,isDark()?Gdiplus::Color(255,255,255,255):Gdiplus::Color(255,0,0,0),true,true);
        } else if(id==Apply||id==OK) {
            Gdiplus::GraphicsPath path;roundPath(path,0,0,w,h,9);
            Gdiplus::Color color=disabled?(isDark()?Gdiplus::Color(255,62,62,66):Gdiplus::Color(255,230,230,230)):(pressed?Gdiplus::Color(255,15,88,225):blue());
            Gdiplus::SolidBrush brush(color);g.FillPath(&brush,&path);text(g,id==Apply?L"Apply":L"OK",{0,-1,w,h},22,Gdiplus::Color(255,255,255,255),false,true);
        } else {
            bool chosen=id==Up?draft.top:(id==Down?draft.bottom:draft.startup);
            Gdiplus::SolidBrush outer(pale());g.FillEllipse(&outer,0.0f,5.5f,27.0f,27.0f);
            if(chosen){Gdiplus::SolidBrush inner(blue());g.FillEllipse(&inner,6.0f,11.5f,15.0f,15.0f);}
            text(g,id==Up?L"Up":(id==Down?L"Down":L"Start Windows"),{40,-1,w-40,h},26,label());
        }
    }
    static void drawButton(DRAWITEMSTRUCT* draw) {
        buffered(draw->hDC,draw->rcItem.right-draw->rcItem.left,draw->rcItem.bottom-draw->rcItem.top,[&](HDC dc){paintButton(draw,dc);});
    }
    static LRESULT sliderDraw(NMCUSTOMDRAW* draw) {
        if(draw->dwDrawStage!=CDDS_PREPAINT)return CDRF_DODEFAULT;
        HWND slider=draw->hdr.hwndFrom;RECT channel{},thumb{};SendMessageW(slider,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));SendMessageW(slider,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        float y=(thumb.top+thumb.bottom)*0.5f,x=(thumb.left+thumb.right)*0.5f;
        RECT bounds{};GetClientRect(slider,&bounds);HDC buffer=CreateCompatibleDC(draw->hdc);HBITMAP bitmap=CreateCompatibleBitmap(draw->hdc,bounds.right,bounds.bottom);
        if(!buffer||!bitmap){if(bitmap)DeleteObject(bitmap);if(buffer)DeleteDC(buffer);return CDRF_DODEFAULT;}
        HGDIOBJ old=SelectObject(buffer,bitmap);
        {
            Gdiplus::Graphics g(buffer);g.Clear(face());g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::Pen track(isDark()?Gdiplus::Color(255,70,70,74):Gdiplus::Color(255,230,230,230),4.46f*factor());track.SetStartCap(Gdiplus::LineCapRound);track.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&track,float(channel.left),y,float(channel.right),y);Gdiplus::Pen fill(isDark()?Gdiplus::Color(255,150,150,155):Gdiplus::Color(255,116,115,115),4.46f*factor());fill.SetStartCap(Gdiplus::LineCapRound);fill.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&fill,float(channel.left),y,x,y);
            Gdiplus::SolidBrush brush(blue());float r=5.58f*factor();g.FillEllipse(&brush,x-r,y-r,2*r,2*r);
        }
        BitBlt(draw->hdc,0,0,bounds.right,bounds.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
        return CDRF_SKIPDEFAULT;
    }
    static void paintSurface(HDC target) {
        Gdiplus::Graphics g(target);g.Clear(face());g.ScaleTransform(factor(),factor());g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen divider(isDark()?Gdiplus::Color(255,62,62,65):Gdiplus::Color(255,230,230,230),1);g.DrawLine(&divider,479.5f,8.5f,479.5f,120.5f);
        auto up=std::to_wstring(draft.up)+L"%",down=std::to_wstring(draft.down)+L"%";
        text(g,up.c_str(),{402,23,65,36},19,label());text(g,down.c_str(),{402,65,65,36},19,label());
    }
    static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM a,LPARAM b) {
        if(msg==WM_CREATE){window=w;dpi=GetDpiForWindow(w);draft=readSettings();if(settingsFileOverride.empty())draft.startup=startupEnabled();if(!draft.enabled){draft.top=false;draft.bottom=false;}applied=draft;controls();shape();return 0;}
        if(msg==WM_DRAWITEM){drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(b));return TRUE;}
        if(msg==WM_NOTIFY){auto h=reinterpret_cast<NMHDR*>(b);if(h->code==NM_CUSTOMDRAW&&(h->idFrom==UpSlider||h->idFrom==DownSlider))return sliderDraw(reinterpret_cast<NMCUSTOMDRAW*>(b));}
        if(msg==WM_COMMAND&&HIWORD(a)==BN_CLICKED) {
            int id=LOWORD(a);
            if(id==OK){EnableWindow(item(OK),FALSE);UpdateWindow(item(OK));DestroyWindow(w);return 0;}
            if(id==Apply){if(dirty())commit();return 0;}
            if(id==Up){draft.enabled=true;draft.top=!draft.top;if(draft.top&&!draft.up)draft.up=50;SendMessageW(item(UpSlider),TBM_SETPOS,TRUE,draft.up);}
            else if(id==Down){draft.enabled=true;draft.bottom=!draft.bottom;if(draft.bottom&&!draft.down)draft.down=50;SendMessageW(item(DownSlider),TBM_SETPOS,TRUE,draft.down);}
            else if(id==Startup)draft.startup=!draft.startup;
            else if(id==Logo){RECT r{};GetWindowRect(item(Logo),&r);appearanceMenu({r.left,r.bottom});return 0;}
            updated(true);return 0;
        }
        if(msg==WM_HSCROLL) {
            HWND slider=reinterpret_cast<HWND>(b);int value=int(SendMessageW(slider,TBM_GETPOS,0,0));
            draft.enabled=true;
            if(slider==item(UpSlider)){draft.up=value;draft.top=value>0;}else if(slider==item(DownSlider)){draft.down=value;draft.bottom=value>0;}else return 0;
            updated(true);return 0;
        }
        if(msg==WM_PAINT) {
            PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT bounds{};GetClientRect(w,&bounds);
            buffered(dc,bounds.right,bounds.bottom,paintSurface,&ps.rcPaint);EndPaint(w,&ps);return 0;
        }
        if(msg==WM_PRINTCLIENT){RECT r{};GetClientRect(w,&r);buffered(reinterpret_cast<HDC>(a),r.right,r.bottom,paintSurface);return 0;}
        if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN){SetBkMode(reinterpret_cast<HDC>(a),TRANSPARENT);return reinterpret_cast<LRESULT>(faceBrush);}
        if(msg==WM_ERASEBKGND)return 1;
        if(msg==WM_NCHITTEST){POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};ScreenToClient(w,&p);if(ChildWindowFromPointEx(w,p,CWP_SKIPINVISIBLE)==w)return HTCAPTION;}
        if(msg==WM_CONTEXTMENU){POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};if(p.x==-1)GetCursorPos(&p);appearanceMenu(p);return 0;}
        if(msg==WM_NCRBUTTONUP){POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};appearanceMenu(p);return 0;}
        if(msg==WM_SETTINGCHANGE){updated();return 0;}
        if(msg==WM_DPICHANGED){dpi=HIWORD(a);RECT* r=reinterpret_cast<RECT*>(b);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);HWND c=GetWindow(w,GW_CHILD);while(c){HWND next=GetWindow(c,GW_HWNDNEXT);DestroyWindow(c);c=next;}controls();shape();return 0;}
        if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
        if(msg==WM_DESTROY){rollbackPreview();if(backdropWindow){SetWindowLongPtrW(w,GWLP_HWNDPARENT,0);DestroyWindow(backdropWindow);backdropWindow=nullptr;}PostQuitMessage(0);return 0;}
        return DefWindowProcW(w,msg,a,b);
    }
}
static int showSettings() {
    HANDLE mutex=CreateMutexW(nullptr,FALSE,objectName(L"Settings").c_str());if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){HWND w=FindWindowW(L"GlassCorners.Settings",nullptr);if(w){ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);}CloseHandle(mutex);return 0;}
    if(!FindWindowW(L"GlassCorners.Control",L"GlassCorners controller")){PROCESS_INFORMATION pi{};if(spawn(L"--supervise --rounded --external",pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}}
    Gdiplus::GdiplusStartupInput startup;if(Gdiplus::GdiplusStartup(&CornerUI::graphicsToken,&startup,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Cannot initialize UI graphics");
    if(!loadPrivateFonts())throw std::runtime_error("Cannot load SF Pro Display fonts");
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
    HINSTANCE instance=GetModuleHandleW(nullptr);HICON icon=brandIcon();WNDCLASSW c{};c.lpfnWndProc=CornerUI::proc;c.hInstance=instance;c.lpszClassName=L"GlassCorners.Settings";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=icon;
    if(!RegisterClassW(&c))throw std::runtime_error("Cannot register corner settings window");
    UINT dpi=GetDpiForSystem();int width=MulDiv(920,dpi,96),height=MulDiv(159,dpi,96);
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO info{};info.cbSize=sizeof(info);GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&info);
    HWND w=CreateWindowExW(WS_EX_APPWINDOW,c.lpszClassName,L"corner",WS_POPUP|WS_CLIPCHILDREN,info.rcWork.left+(info.rcWork.right-info.rcWork.left-width)/2,info.rcWork.top+(info.rcWork.bottom-info.rcWork.top-height)/2,width,height,nullptr,nullptr,instance,nullptr);
    if(!w)throw std::runtime_error("Cannot create corner settings window");
    ShowWindow(w,SW_SHOW);SetForegroundWindow(w);
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(w);continue;}if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN){SendMessageW(w,WM_COMMAND,GetFocus()==CornerUI::item(CornerUI::Apply)?CornerUI::Apply:CornerUI::OK,0);continue;}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(CornerUI::logoIcon)DestroyIcon(CornerUI::logoIcon);
    if(CornerUI::faceBrush)DeleteObject(CornerUI::faceBrush);
    DestroyIcon(icon);Gdiplus::GdiplusShutdown(CornerUI::graphicsToken);CloseHandle(mutex);return 0;
}
