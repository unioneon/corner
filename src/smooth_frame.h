// The opaque native controls and their alpha edge share one contour. Windows
// regions alone are binary, so a layered owner supplies subpixel coverage.
namespace SmoothFrame {
    static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM a,LPARAM b) {
        if(msg==WM_NCHITTEST)return HTTRANSPARENT;
        if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        return DefWindowProcW(w,msg,a,b);
    }
    struct Surface {
        HWND backdrop=nullptr,originalOwner=nullptr;
        int width=0,height=0,padding=0;
        bool shadowEnabled=false;
        std::vector<BYTE> alpha;
        std::vector<DWORD> pixels;
        void sync(HWND window,bool visible) {
            if(!backdrop)return;
            if(!visible||IsIconic(window)){ShowWindow(backdrop,SW_HIDE);return;}
            RECT r{};GetWindowRect(window,&r);
            SetWindowPos(backdrop,window,r.left-padding,r.top-padding,0,0,
                SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }
        void release(HWND window) {
            if(!backdrop)return;
            SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(originalOwner));
            DestroyWindow(backdrop);backdrop=nullptr;
        }
        template<class Contour> bool rebuild(HWND window,float scale,Gdiplus::Color color,bool topmost,bool shadow,Contour contour) {
            RECT bounds{},position{};GetClientRect(window,&bounds);GetWindowRect(window,&position);
            width=bounds.right;height=bounds.bottom;if(width<1||height<1)return false;
            if(!backdrop) {
                WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"corner.SmoothFrame";RegisterClassW(&cls);
                backdrop=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|(topmost?WS_EX_TOPMOST:0),cls.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,cls.hInstance,nullptr);
                if(!backdrop)return false;
                originalOwner=reinterpret_cast<HWND>(GetWindowLongPtrW(window,GWLP_HWNDPARENT));
                SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(backdrop));
            }
            constexpr int samples=4;
            Gdiplus::Bitmap high(width*samples,height*samples,PixelFormat32bppPARGB);
            {Gdiplus::Graphics g(&high);g.Clear(Gdiplus::Color(0,0,0,0));g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.ScaleTransform(scale*samples,scale*samples);Gdiplus::GraphicsPath path;contour(path);Gdiplus::SolidBrush brush(Gdiplus::Color(255,255,255,255));g.FillPath(&brush,&path);}
            Gdiplus::Rect highRect(0,0,width*samples,height*samples);Gdiplus::BitmapData data{};
            if(high.LockBits(&highRect,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok)return false;
            alpha.resize(size_t(width)*height);std::vector<RECT> spans;
            for(int y=0;y<height;y++) {
                int first=width,last=-1;
                for(int x=0;x<width;x++) {
                    unsigned sum=0;
                    for(int sy=0;sy<samples;sy++) {
                        const BYTE* row=static_cast<const BYTE*>(data.Scan0)+(y*samples+sy)*data.Stride;
                        for(int sx=0;sx<samples;sx++)sum+=row[(x*samples+sx)*4+3];
                    }
                    BYTE a=BYTE((sum+samples*samples/2)/(samples*samples));alpha[size_t(y)*width+x]=a;
                    if(a==255){first=std::min(first,x);last=x;}
                }
                if(last>=first)spans.push_back({first,y,last+1,y+1});
            }
            high.UnlockBits(&data);
            shadowEnabled=shadow;padding=int(std::ceil(12*scale));
            int fullWidth=width+padding*2,fullHeight=height+padding*2;
            std::vector<float> shade(size_t(fullWidth)*fullHeight,0);
            if(shadow) {
                int offset=int(std::lround(2*scale));
                for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
                    int dy=y+padding+offset;if(dy<fullHeight)shade[size_t(dy)*fullWidth+x+padding]=alpha[size_t(y)*width+x]/255.0f;
                }
                float sigma=3.5f*scale;int radius=int(std::ceil(sigma*3));
                std::vector<float> kernel(radius*2+1),temp(shade.size());float sum=0;
                for(int k=-radius;k<=radius;k++){float v=std::exp(-(k*k)/(2*sigma*sigma));kernel[k+radius]=v;sum+=v;}
                for(auto& v:kernel)v/=sum;
                for(int y=0;y<fullHeight;y++)for(int x=0;x<fullWidth;x++) {
                    float v=0;for(int k=-radius;k<=radius;k++)if(x+k>=0&&x+k<fullWidth)v+=shade[size_t(y)*fullWidth+x+k]*kernel[k+radius];
                    temp[size_t(y)*fullWidth+x]=v;
                }
                for(int y=0;y<fullHeight;y++)for(int x=0;x<fullWidth;x++) {
                    float v=0;for(int k=-radius;k<=radius;k++)if(y+k>=0&&y+k<fullHeight)v+=temp[size_t(y+k)*fullWidth+x]*kernel[k+radius];
                    shade[size_t(y)*fullWidth+x]=v*0.16f;
                }
            }
            pixels.resize(shade.size());
            for(int y=0;y<fullHeight;y++)for(int x=0;x<fullWidth;x++) {
                int cx=x-padding,cy=y-padding;unsigned a=(cx>=0&&cx<width&&cy>=0&&cy<height)?alpha[size_t(cy)*width+cx]:0;
                unsigned combined=a+unsigned(std::lround(shade[size_t(y)*fullWidth+x]*(255-a)));
                pixels[size_t(y)*fullWidth+x]=(combined<<24)|(((color.GetR()*a+127)/255)<<16)|(((color.GetG()*a+127)/255)<<8)|((color.GetB()*a+127)/255);
            }
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=fullWidth;info.bmiHeader.biHeight=-fullHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            HDC dc=CreateCompatibleDC(nullptr);void* raw=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0);
            if(!dc||!bitmap||!raw){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return false;}
            CopyMemory(raw,pixels.data(),pixels.size()*sizeof(DWORD));HGDIOBJ old=SelectObject(dc,bitmap);
            POINT dst{position.left-padding,position.top-padding},src{};SIZE size{fullWidth,fullHeight};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            BOOL ok=UpdateLayeredWindow(backdrop,nullptr,&dst,&size,dc,&src,0,&blend,ULW_ALPHA);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
            if(!ok||spans.empty())return false;
            std::vector<BYTE> regionData(sizeof(RGNDATAHEADER)+spans.size()*sizeof(RECT));auto region=reinterpret_cast<RGNDATA*>(regionData.data());
            region->rdh={sizeof(RGNDATAHEADER),RDH_RECTANGLES,DWORD(spans.size()),DWORD(spans.size()*sizeof(RECT)),{0,0,width,height}};
            CopyMemory(region->Buffer,spans.data(),spans.size()*sizeof(RECT));HRGN native=ExtCreateRegion(nullptr,DWORD(regionData.size()),region);
            if(!native)return false;
            SetWindowRgn(window,native,TRUE);sync(window,IsWindowVisible(window)!=FALSE);return true;
        }
    };
}
