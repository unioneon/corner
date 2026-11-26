// All coordinates and mask sizes are physical desktop pixels. A mirrored
// desktop has one logical rectangle, even when CCD reports several targets.
static bool sameRect(const RECT& a,const RECT& b) {
    return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;
}
static std::vector<Panel> choosePanels(const std::vector<Panel>& available,bool roundedMode,bool demoMode,bool external,const std::wstring& path,int width,int height) {
    std::vector<Panel> result;
    for(const auto& panel:available) {
        if(!path.empty()&&panel.path!=path)continue;
        if(!roundedMode&&!demoMode&&!panel.internal&&!external)continue;
        if(panel.rect.right<=panel.rect.left||panel.rect.bottom<=panel.rect.top)continue;
        bool duplicate=false;for(const auto& chosen:result)if(sameRect(chosen.rect,panel.rect)){duplicate=true;break;}
        if(!duplicate)result.push_back(panel);
    }
    if(!roundedMode&&!demoMode) {
        if(result.size()!=1)return {};
        const RECT& r=result.front().rect;
        if(r.right-r.left!=width||r.bottom-r.top!=height)return {};
    }
    return result;
}
static POINT cornerOrigin(const RECT& display,int side,int width,int height) {
    return {side%2?display.right-width:display.left,side<2?display.bottom-height:display.top};
}
static bool coversDisplay(const RECT& window,const RECT& display) {
    return window.left<=display.left&&window.top<=display.top&&window.right>=display.right&&window.bottom>=display.bottom;
}
static bool showDisplayCorners(const Settings& s,bool full) {
    return s.enabled&&(s.game||!s.hideFullscreen||!full);
}
