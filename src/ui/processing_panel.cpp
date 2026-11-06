#include "processing_panel.h"
#include "core/processing_status.h"
#include <commctrl.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <memory>
#include <string>

namespace fourbyfour::ui {
namespace {
constexpr int Width=600,Height=488;
struct Panel {
    Controls controls;HWND window{},phase{},progress{},metrics{},memory{},mode{},quality{},mix{},gain{},bypass{},mixValue{},gainValue{},profile{},channels{},match{},strength{},strengthValue{};
    HFONT font{},title{};HBRUSH background{},card{};bool marquee{},owned{};
    IDXGIAdapter3* adapter{};ULONGLONG memoryTick{};
    ~Panel(){if(adapter)adapter->Release();if(font)DeleteObject(font);if(title)DeleteObject(title);if(background)DeleteObject(background);if(card)DeleteObject(card);}
    HWND add(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id) {
        auto control=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return control;
    }
    void initialize(HWND owner) {
        // The host owns this fixed-size VST view's scaling. Keep fonts in the
        // same logical coordinate system as controls to avoid DPI clipping.
        window=owner;const int dpi=96;
        font=CreateFontW(-MulDiv(11,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        title=CreateFontW(-MulDiv(20,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        background=CreateSolidBrush(RGB(13,21,24));card=CreateSolidBrush(RGB(23,36,40));
        auto heading=add(L"STATIC",L"4x4Tools",0,24,18,540,34,0);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(title),TRUE);
        add(L"STATIC",processing::name(controls.effect),0,26,57,540,26,0);
        int y=92;
        if(controls.qualityChoice) {
            add(L"STATIC",L"Processing",0,26,y+3,110,26,0);
            quality=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,142,y,426,120,101);
            if(controls.voice)SendMessageW(quality,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Auto · Preview while editing / HQ in offline render"));
            SendMessageW(quality,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Interactive Preview"));
            SendMessageW(quality,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Highest Quality · Render"));
            SendMessageW(quality,CB_SETCURSEL,controls.voice?controls.quality:std::clamp(controls.quality-1,0,1),0);
        }
        if(controls.voice) {
            mix=add(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,134,132,276,32,102);SendMessageW(mix,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(mix,TBM_SETPOS,TRUE,static_cast<int>(controls.mix*100));
            add(L"STATIC",L"Mix (%)",0,26,139,100,22,0);
            mixValue=add(L"STATIC",L"",0,416,139,48,22,0);
            gain=add(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,134,169,276,32,103);SendMessageW(gain,TBM_SETRANGE,TRUE,MAKELPARAM(0,360));SendMessageW(gain,TBM_SETPOS,TRUE,static_cast<int>((controls.gainDb+24)*10));
            add(L"STATIC",L"Gain (dB)",0,26,176,100,22,0);
            gainValue=add(L"STATIC",L"",0,416,176,72,22,0);readouts();
            bypass=add(L"BUTTON",L"Bypass",BS_AUTOCHECKBOX|WS_TABSTOP,466,136,110,28,104);SendMessageW(bypass,BM_SETCHECK,controls.bypass?BST_CHECKED:BST_UNCHECKED,0);
            if(controls.microphoneChoice) {
                add(L"STATIC",L"Microphone",0,26,208,104,24,0);
                profile=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,142,203,426,150,106);
                for(const auto* text:{L"Natural",L"Bright",L"Vibrant",L"Flat",L"Full",L"Thin",L"Warm"})SendMessageW(profile,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
                SendMessageW(profile,CB_SETCURSEL,controls.microphone,0);
            }
            if(controls.strengthChoice){
                add(L"STATIC",L"Suppression",0,26,213,104,24,0);
                strength=add(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,134,206,276,32,109);SendMessageW(strength,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(strength,TBM_SETPOS,TRUE,static_cast<int>(controls.strength*100));
                strengthValue=add(L"STATIC",L"",0,416,213,48,22,0);readouts();
            }
            if(controls.channelChoice){
                add(L"STATIC",L"Voice channels",0,26,246,104,24,0);
                channels=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,142,241,426,100,107);
                for(const auto* text:{L"Independent stereo",L"Dialogue · one centred voice"})SendMessageW(channels,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
                SendMessageW(channels,CB_SETCURSEL,controls.channelMode,0);
                match=add(L"BUTTON",L"Match input level",BS_AUTOCHECKBOX|WS_TABSTOP,142,276,220,25,108);
                SendMessageW(match,BM_SETCHECK,controls.matchLevel?BST_CHECKED:BST_UNCHECKED,0);
            }
        } else add(L"STATIC",L"Preview reduces model work. Highest Quality uses the full render path.\nApply the quality choice before the final render.",0,26,137,540,58,0);
        const int offset=controls.channelChoice?72:0;
        mode=add(L"STATIC",L"Waiting for an Adobe processing request",0,26,252+offset,540,24,0);
        phase=add(L"STATIC",L"Ready",0,26,282+offset,540,24,0);
        progress=add(PROGRESS_CLASSW,L"",PBS_SMOOTH,26,312+offset,540,17,0);SendMessageW(progress,PBM_SETRANGE32,0,100);
        metrics=add(L"STATIC",L"",0,26,339+offset,540,28,0);
        memory=add(L"STATIC",L"GPU memory: checking…",0,26,375+offset,540,28,0);
        add(L"BUTTON",L"Cancel current work",WS_TABSTOP,26,423+offset,174,32,105);
        add(L"STATIC",L"Cancellation waits for the current NVIDIA model step.",0,216,426+offset,354,42,0);
        IDXGIFactory1* factory{};
        if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            for(UINT i=0;!adapter;++i){IDXGIAdapter1* item{};if(factory->EnumAdapters1(i,&item)==DXGI_ERROR_NOT_FOUND)break;if(!item)break;
                DXGI_ADAPTER_DESC1 desc{};item->GetDesc1(&desc);if(desc.VendorId==0x10de)item->QueryInterface(IID_PPV_ARGS(&adapter));item->Release();}
            factory->Release();
        }
        SetTimer(window,1,150,nullptr);
    }
    void readouts(){wchar_t text[40]{};swprintf_s(text,L"%.0f",controls.mix*100);SetWindowTextW(mixValue,text);swprintf_s(text,L"%+.1f",controls.gainDb);SetWindowTextW(gainValue,text);if(strengthValue){swprintf_s(text,L"%.0f",controls.strength*100);SetWindowTextW(strengthValue,text);}}
    void refresh() {
        if(controls.synchronize) {
            controls.synchronize(controls);
            if(quality)SendMessageW(quality,CB_SETCURSEL,controls.voice?controls.quality:std::clamp(controls.quality-1,0,1),0);
            if(mix)SendMessageW(mix,TBM_SETPOS,TRUE,static_cast<int>(controls.mix*100));
            if(gain)SendMessageW(gain,TBM_SETPOS,TRUE,static_cast<int>((controls.gainDb+24)*10));
            if(bypass)SendMessageW(bypass,BM_SETCHECK,controls.bypass?BST_CHECKED:BST_UNCHECKED,0);
            if(profile)SendMessageW(profile,CB_SETCURSEL,controls.microphone,0);
            if(channels)SendMessageW(channels,CB_SETCURSEL,controls.channelMode,0);
            if(match)SendMessageW(match,BM_SETCHECK,controls.matchLevel?BST_CHECKED:BST_UNCHECKED,0);
            if(strength)SendMessageW(strength,TBM_SETPOS,TRUE,static_cast<int>(controls.strength*100));
            if(mix&&gain)readouts();
        }
        if(GetTickCount64()-memoryTick>1000) {
            memoryTick=GetTickCount64();DXGI_QUERY_VIDEO_MEMORY_INFO info{};wchar_t text[192]{};
            if(adapter&&SUCCEEDED(adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info)))
                swprintf_s(text,L"GPU memory · this Adobe process: %.1f / %.1f GB budget%s",info.CurrentUsage/1073741824.,info.Budget/1073741824.,info.Budget&&info.CurrentUsage>info.Budget*.85?L" · HIGH PRESSURE":L"");
            else wcscpy_s(text,L"GPU memory usage is unavailable");SetWindowTextW(memory,text);
        }
        processing::Status status;if(!processing::read(controls.effect,status))return;
        if(!status.started)return;
        SetWindowTextW(mode,status.mode);SetWindowTextW(phase,status.active&&processing::cancelled(controls.effect)?L"Cancel requested · finishing the current model step…":status.phase);
        const bool unknown=status.active&&status.percent<0;
        if(unknown!=marquee){SetWindowLongPtrW(progress,GWL_STYLE,WS_CHILD|WS_VISIBLE|(unknown?PBS_MARQUEE:PBS_SMOOTH));SendMessageW(progress,PBM_SETMARQUEE,unknown,40);marquee=unknown;}
        if(!unknown)SendMessageW(progress,PBM_SETPOS,std::max(0,status.percent),0);
        wchar_t text[160]{};const auto elapsed=status.active?GetTickCount64()-status.started:status.elapsed;
        if(status.active)swprintf_s(text,L"Elapsed %.1f s%s",elapsed/1000.,status.percent<0?L" · NVIDIA does not expose a percentage for this step":L"");
        else swprintf_s(text,L"Last request %.1f ms",double(elapsed));
        SetWindowTextW(metrics,text);
    }
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* panel=reinterpret_cast<Panel*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){auto* create=reinterpret_cast<CREATESTRUCTW*>(l);panel=static_cast<Panel*>(create->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(panel));panel->window=window;}
    if(!panel)return DefWindowProcW(window,message,w,l);
    switch(message) {
    case WM_CREATE:panel->initialize(window);return 0;
    case WM_TIMER:panel->refresh();return 0;
    case WM_COMMAND:
        if(LOWORD(w)==105){processing::cancel(panel->controls.effect);SetWindowTextW(panel->phase,L"Cancel requested · finishing the current model step…");return 0;}
        if(LOWORD(w)==101&&HIWORD(w)==CBN_SELCHANGE) {
            const int selected=static_cast<int>(SendMessageW(panel->quality,CB_GETCURSEL,0,0));
            panel->controls.quality=panel->controls.voice?selected:selected+1;
            if(panel->controls.changed)panel->controls.changed(0,panel->controls.quality);return 0;
        }
        if(LOWORD(w)==104&&panel->controls.changed)panel->controls.changed(3,SendMessageW(panel->bypass,BM_GETCHECK,0,0)==BST_CHECKED?1.:0.);
        if(LOWORD(w)==106&&HIWORD(w)==CBN_SELCHANGE&&panel->controls.changed)panel->controls.changed(4,double(SendMessageW(reinterpret_cast<HWND>(l),CB_GETCURSEL,0,0)));
        if(LOWORD(w)==107&&HIWORD(w)==CBN_SELCHANGE&&panel->controls.changed)panel->controls.changed(5,double(SendMessageW(panel->channels,CB_GETCURSEL,0,0)));
        if(LOWORD(w)==108&&panel->controls.changed)panel->controls.changed(6,SendMessageW(panel->match,BM_GETCHECK,0,0)==BST_CHECKED?1.:0.);
        break;
    case WM_HSCROLL:
        if(panel->controls.changed) {
            const bool mix=reinterpret_cast<HWND>(l)==panel->mix;
            const bool strength=reinterpret_cast<HWND>(l)==panel->strength;
            const auto value=SendMessageW(reinterpret_cast<HWND>(l),TBM_GETPOS,0,0);
            if(strength)panel->controls.strength=value/100.;else if(mix)panel->controls.mix=value/100.;else panel->controls.gainDb=value/10.-24.;panel->readouts();
            panel->controls.changed(strength?7:mix?1:2,(strength||mix)?value/100.:value/10.-24.);
        }return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(w),RGB(223,241,239));SetBkColor(reinterpret_cast<HDC>(w),RGB(13,21,24));return reinterpret_cast<LRESULT>(panel->background);
    case WM_ERASEBKGND:{RECT rect;GetClientRect(window,&rect);FillRect(reinterpret_cast<HDC>(w),&rect,panel->background);return 1;}
    case WM_DESTROY:KillTimer(window,1);return 0;
    case WM_NCDESTROY:SetWindowLongPtrW(window,GWLP_USERDATA,0);if(panel->owned)delete panel;return DefWindowProcW(window,message,w,l);
    }
    return DefWindowProcW(window,message,w,l);
}
const wchar_t* className() {
    // Each plugin links this code privately. Register a class bound to that
    // module, preventing another effect's unload from invalidating the WndProc.
    static wchar_t name[80]{};static bool ready=false;
    if(!ready){swprintf_s(name,L"4x4ToolsPanel-%p",reinterpret_cast<void*>(&procedure));WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=procedure;cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=name;RegisterClassExW(&cls);ready=true;}
    return name;
}
}
HWND createPanel(HWND parent,const Controls& controls) {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_PROGRESS_CLASS|ICC_BAR_CLASSES};InitCommonControlsEx(&common);
    auto panel=std::make_unique<Panel>();panel->controls=controls;
    auto window=CreateWindowExW(0,className(),L"4x4Tools Processing",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,Width,Height+(controls.channelChoice?72:0),parent,nullptr,GetModuleHandleW(nullptr),panel.get());
    if(window){panel->owned=true;panel.release();}return window;
}
int showPanel(int effect,int quality,bool choice) {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_PROGRESS_CLASS|ICC_BAR_CLASSES};InitCommonControlsEx(&common);
    const auto owner=GetActiveWindow();int selected=quality;auto panel=std::make_unique<Panel>();
    panel->controls={effect,quality,1,0,false,choice,false,[&](int control,double value){if(control==0)selected=static_cast<int>(value);}};
    RECT rect{0,0,Width,Height};AdjustWindowRectEx(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME);
    auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,className(),L"4x4Tools · Processing",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,owner,nullptr,GetModuleHandleW(nullptr),panel.get());
    if(!window)return quality;panel->owned=true;panel.release();if(owner)EnableWindow(owner,FALSE);
    MSG message{};while(IsWindow(window)){const auto result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(!result)PostQuitMessage(static_cast<int>(message.wParam));break;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    if(IsWindow(window))DestroyWindow(window);if(owner){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return selected;
}
}
