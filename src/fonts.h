static std::vector<HANDLE> privateFonts;
static const wchar_t* uiFontFamily=L"Segoe UI";
static bool fontsLoaded=false;
static bool loadPrivateFonts() {
    if(fontsLoaded)return true;
    for(int id:{201,202}) {
        HRSRC resource=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(id),RT_RCDATA);
        if(!resource)continue;
        HGLOBAL loaded=LoadResource(GetModuleHandleW(nullptr),resource);DWORD count=0;
        HANDLE handle=AddFontMemResourceEx(LockResource(loaded),SizeofResource(GetModuleHandleW(nullptr),resource),nullptr,&count);
        if(handle&&count)privateFonts.push_back(handle);
    }
    auto exe=exePath();auto folder=exe.substr(0,exe.find_last_of(L"\\/"))+L"\\fonts\\";
    for(const wchar_t* name:{L"SF-Pro-Display-Regular.otf",L"SF-Pro-Display-Bold.otf"}) {
        auto file=folder+name;if(GetFileAttributesW(file.c_str())!=INVALID_FILE_ATTRIBUTES)AddFontResourceExW(file.c_str(),FR_PRIVATE,nullptr);
    }
    HDC dc=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-24,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"SF Pro Display");
    HGDIOBJ old=SelectObject(dc,font);wchar_t face[128]{};GetTextFaceW(dc,128,face);SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
    if(!_wcsicmp(face,L"SF Pro Display"))uiFontFamily=L"SF Pro Display";
    fontsLoaded=true;
    return true;
}
static void drawSF(Gdiplus::Graphics& graphics,const wchar_t* value,Gdiplus::RectF r,float size,Gdiplus::Color color,bool bold,bool center,float scale) {
    graphics.Flush(Gdiplus::FlushIntentionSync);HDC dc=graphics.GetHDC();
    HFONT font=CreateFontW(-int(std::lround(size*scale)),0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,uiFontFamily);
    HGDIOBJ old=SelectObject(dc,font);int backgroundMode=SetBkMode(dc,TRANSPARENT);COLORREF oldColor=SetTextColor(dc,RGB(color.GetR(),color.GetG(),color.GetB()));
    RECT rect{int(std::lround(r.X*scale)),int(std::lround(r.Y*scale)),int(std::lround((r.X+r.Width)*scale)),int(std::lround((r.Y+r.Height)*scale))};
    DrawTextW(dc,value,-1,&rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|(center?DT_CENTER:DT_LEFT));
    SetTextColor(dc,oldColor);SetBkMode(dc,backgroundMode);SelectObject(dc,old);DeleteObject(font);graphics.ReleaseHDC(dc);
}
