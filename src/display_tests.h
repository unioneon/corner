// Synthetic layouts exercise real hidden HWNDs; they do not emulate a display
// driver or claim that two physical monitors were available for validation.
static void testMultiMonitor() {
    Panel primary{L"display-a",L"panel-a",L"Primary",{0,0,2560,1440},true};
    Panel left{L"display-b",L"panel-b",L"Left",{-1920,120,0,1200},false};
    Panel above{L"display-c",L"panel-c",L"Above",{0,-2160,3840,0},false};
    Panel portrait{L"display-d",L"panel-d",L"Portrait",{2560,-300,3640,1620},false};
    auto choose=[](const std::vector<Panel>& list,const std::wstring& path=L""){return choosePanels(list,true,false,true,path,0,0);};
    if(choose({primary,left}).size()!=2||choose({left}).size()!=1||choose({primary,left},L"panel-b").size()!=1||!choose({primary},L"missing").empty())
        throw std::runtime_error("Multiple display selection failed");
    Panel mirrored=primary;mirrored.device=L"clone";mirrored.path=L"clone-path";
    if(choose({primary,mirrored}).size()!=1)throw std::runtime_error("Mirrored displays were not deduplicated");
    if(choosePanels({primary},false,false,false,L"",2560,1440).size()!=1||!choosePanels({primary,left},false,false,true,L"",2560,1440).empty())
        throw std::runtime_error("Exact-mask display selection changed");
    POINT point=cornerOrigin(left.rect,0,64,64);
    if(point.x!=-1920||point.y!=1136)throw std::runtime_error("Negative display origin failed");
    point=cornerOrigin(portrait.rect,3,64,64);
    if(point.x!=3576||point.y!=-300)throw std::runtime_error("Portrait corner placement failed");
    point=cornerOrigin(above.rect,2,64,64);
    if(point.x!=0||point.y!=-2160)throw std::runtime_error("Vertical display layout failed");
    Settings policy;policy.hideFullscreen=true;policy.game=false;
    bool firstFull=coversDisplay(primary.rect,primary.rect),secondFull=coversDisplay(primary.rect,left.rect);
    if(!firstFull||secondFull||showDisplayCorners(policy,firstFull)||!showDisplayCorners(policy,secondFull))
        throw std::runtime_error("Fullscreen hid an unrelated monitor");
    policy.game=true;if(!showDisplayCorners(policy,firstFull))throw std::runtime_error("Game mode did not override fullscreen hiding");
    WNDCLASSW cls{};cls.lpfnWndProc=cornerProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"GlassCorners.Corner";
    if(!RegisterClassW(&cls))throw std::runtime_error("Cannot register multi-display test class");
    State testState;State* previousState=state;state=&testState;
    debugMode=true;rounded=true;cornerCount=4;
    masks[0]=fixture(false);masks[1]=fixture(true);masks[2]=masks[0];masks[3]=masks[1];
    {
        std::vector<DisplayCorners> current;
        auto replace=[&](const std::vector<Panel>& list) {
            std::vector<HWND> old;for(const auto& display:current)for(HWND w:display.windows)if(w)old.push_back(w);
            {auto next=createDisplays(choose(list));current.swap(next);}
            for(HWND w:old)if(IsWindow(w))throw std::runtime_error("Stale corners survived a topology change");
            for(const auto& display:current)for(int side=0;side<4;side++) {
                HWND w=display.windows[side];RECT rect{};GetWindowRect(w,&rect);
                POINT expected=cornerOrigin(display.panel.rect,side,masks[side].w,masks[side].h);
                if(rect.left!=expected.x||rect.top!=expected.y||rect.right-rect.left!=masks[side].w||rect.bottom-rect.top!=masks[side].h)
                    throw std::runtime_error("Native multi-display window geometry failed");
                if(IsWindowVisible(w))throw std::runtime_error("Synthetic display test unexpectedly showed a window");
            }
        };
        replace({primary,left});if(current.size()!=2)throw std::runtime_error("Two displays did not create eight corners");
        replace({primary});if(current.size()!=1)throw std::runtime_error("Disconnect did not remove second display");
        replace({primary,left,portrait});if(current.size()!=3)throw std::runtime_error("Reconnect did not restore displays");
        replace({primary,above});
        replace({primary,mirrored});if(current.size()!=1)throw std::runtime_error("Mirrored output created duplicate windows");
        replace({});if(!current.empty())throw std::runtime_error("No-display transition retained windows");
        replace({left});
    }
    state=previousState;UnregisterClassW(cls.lpszClassName,cls.hInstance);
}
