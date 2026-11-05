#include "ui/processing_panel.h"
#include "core/processing_status.h"
#include <commctrl.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
LRESULT CALLBACK parentProc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==WM_CLOSE){DestroyWindow(window);return 0;}if(message==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(window,message,w,l);}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int wmain(int argc,wchar_t** argv) {
    using namespace fourbyfour;
    HWND parent{};
    try {
        const bool visible=argc>1&&std::wstring(argv[1])==L"--preview";
        WNDCLASSW cls{};cls.lpfnWndProc=parentProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"4x4ToolsProcessingFixture";RegisterClassW(&cls);
        RECT bounds{0,0,600,488};AdjustWindowRectEx(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_NOACTIVATE);
        parent=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"4x4Tools · Processing panel preview",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|(visible?WS_VISIBLE:0),80,80,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,cls.hInstance,nullptr);
        require(parent,"Cannot create panel host");int quality=-1,events=0;
        ui::Controls controls;controls.effect=11;controls.voice=true;controls.qualityChoice=true;controls.quality=0;controls.mix=.8;controls.gainDb=-1.5;
        controls.changed=[&](int id,double value){++events;if(id==0)quality=static_cast<int>(value);};
        const auto panel=ui::createPanel(parent,controls);require(panel,"Cannot create processing panel");
        if(visible)ShowWindow(parent,SW_SHOWNOACTIVATE);
        const auto choice=GetDlgItem(panel,101);SendMessageW(choice,CB_SETCURSEL,1,0);SendMessageW(panel,WM_COMMAND,MAKEWPARAM(101,CBN_SELCHANGE),reinterpret_cast<LPARAM>(choice));
        require(quality==1&&events==1,"Native quality selection did not reach the host callback");
        processing::begin(11,L"Interactive Preview");processing::report(11,L"Preparing the selected voice model…",1,3);
        processing::Status status;require(processing::read(11,status)&&status.active&&status.percent==33,"Processing progress was not shared");
        SendMessageW(panel,WM_COMMAND,105,0);require(processing::cancelled(11),"Cancellation did not reach the model status");
        processing::begin(11,L"Interactive Preview");processing::report(11,L"Processing speech · current model step…");
        if(visible) {
            SetTimer(parent,1,120000,nullptr);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_TIMER&&message.hwnd==parent){DestroyWindow(parent);break;}TranslateMessage(&message);DispatchMessageW(&message);}parent=nullptr;
        }
        if(parent){DestroyWindow(parent);parent=nullptr;}
        std::cout<<"Native panel lifecycle, quality callback, shared progress and cancellation passed.\n";return 0;
    }catch(const std::exception& error){if(parent)DestroyWindow(parent);std::cerr<<error.what()<<'\n';return 1;}
}
