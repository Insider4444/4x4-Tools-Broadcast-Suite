#pragma once
#include <windows.h>
#include <functional>

namespace fourbyfour::ui {
struct Controls {
    int effect{};
    int quality{}; // 0 Auto (voice only), 1 Preview, 2 HQ
    double mix{1},gainDb{};
    bool voice{},qualityChoice{},bypass{};
    std::function<void(int,double)> changed; // 0 quality, 1 mix, 2 gain, 3 bypass
    bool microphoneChoice{};
    int microphone{};
    std::function<void(Controls&)> synchronize;
    bool channelChoice{};
    int channelMode{}; // 0 independent stereo, 1 one centred dialogue model
    bool matchLevel{};
    bool strengthChoice{};
    double strength{1};
};
HWND createPanel(HWND parent,const Controls& controls);
// Native Adobe button opens an owned panel; choices return through Adobe's
// parameter callback so quality changes invalidate the host render cache.
int showPanel(int effect,int quality,bool qualityChoice);
}
