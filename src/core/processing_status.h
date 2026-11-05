#pragma once
#include <windows.h>
#include <cstdint>

namespace fourbyfour::processing {
constexpr int VideoBase=0,AudioBase=7,Count=13;
struct Status {
    bool active{};
    int percent{-1};
    std::uint64_t started{},elapsed{};
    wchar_t phase[128]{};
    wchar_t mode[64]{};
};
void begin(int effect,const wchar_t* mode) noexcept;
void report(int effect,const wchar_t* phase,int done=-1,int total=0) noexcept;
void finish(int effect,const wchar_t* phase=L"Ready") noexcept;
bool read(int effect,Status& status) noexcept;
void cancel(int effect) noexcept;
bool cancelled(int effect) noexcept;
const wchar_t* name(int effect) noexcept;
}
