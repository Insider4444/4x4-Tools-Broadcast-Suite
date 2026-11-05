#include "processing_status.h"
#include <algorithm>
#include <cwchar>

namespace fourbyfour::processing {
namespace {
struct Slot { volatile LONG lock{},cancelled{};Status status; };
struct Data { Slot slots[Count]; };
struct Shared {
    HANDLE mapping{};Data* data{};
    Shared() {
        wchar_t id[96]{};swprintf_s(id,L"Local\\4x4Tools-Processing-v1-%lu",GetCurrentProcessId());
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Data),id);
        if(mapping)data=static_cast<Data*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Data)));
    }
    ~Shared(){if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);}
};
Slot* slot(int effect) noexcept {static Shared shared;return shared.data&&effect>=0&&effect<Count?&shared.data->slots[effect]:nullptr;}
struct Lock {Slot* p;explicit Lock(Slot* value):p(value&&InterlockedCompareExchange(&value->lock,1,0)==0?value:nullptr){}~Lock(){if(p)InterlockedExchange(&p->lock,0);}};
}
const wchar_t* name(int effect) noexcept {
    static constexpr const wchar_t* names[]{L"Background Remove",L"Background Blur",L"Studio Light",L"Video Denoise",L"Auto Frame",L"Detail Restore",L"Eye Contact",
        L"Noise Clean",L"Room Echo Clean",L"Voice Clean",L"Studio Voice Preview",L"Studio Voice HQ",L"Voice Restore"};
    return effect>=0&&effect<Count?names[effect]:L"4x4Tools";
}
void begin(int effect,const wchar_t* mode) noexcept {
    auto* p=slot(effect);Lock lock(p);if(!lock.p)return;
    p->status={};p->status.active=true;p->status.percent=-1;p->status.started=GetTickCount64();
    wcsncpy_s(p->status.mode,mode,_TRUNCATE);wcsncpy_s(p->status.phase,L"Preparing…",_TRUNCATE);InterlockedExchange(&p->cancelled,0);
}
void report(int effect,const wchar_t* phase,int done,int total) noexcept {
    Lock lock(slot(effect));if(!lock.p)return;
    wcsncpy_s(lock.p->status.phase,phase,_TRUNCATE);
    lock.p->status.percent=done>=0&&total>0?std::clamp(static_cast<int>(100LL*done/total),0,100):-1;
}
void finish(int effect,const wchar_t* phase) noexcept {
    Lock lock(slot(effect));if(!lock.p)return;
    auto& s=lock.p->status;s.elapsed=GetTickCount64()-s.started;s.active=false;s.percent=100;wcsncpy_s(s.phase,phase,_TRUNCATE);
}
bool read(int effect,Status& status) noexcept {Lock lock(slot(effect));if(!lock.p)return false;status=lock.p->status;return true;}
void cancel(int effect) noexcept {if(auto* p=slot(effect))InterlockedExchange(&p->cancelled,1);}
bool cancelled(int effect) noexcept {auto* p=slot(effect);return p&&InterlockedCompareExchange(&p->cancelled,0,0)!=0;}
}
