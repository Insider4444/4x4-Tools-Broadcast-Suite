#include "audio_contract.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fourbyfour {
AudioContract audioContract(AudioEffect effect,int hostRate) {
    if(hostRate<8000||hostRate>192000||static_cast<unsigned>(effect)>static_cast<unsigned>(AudioEffect::restore))
        throw std::invalid_argument("Unsupported audio effect or host sample rate.");
    AudioContract result;
    result.inputRate=effect==AudioEffect::restore?16000:48000;
    result.outputRate=48000;
    result.inputFrames=effect==AudioEffect::studioQuality?288000:effect==AudioEffect::restore?160:480;
    result.outputFrames=effect==AudioEffect::studioQuality?288000:480;
    const int delay48=effect==AudioEffect::restore?2880:effect==AudioEffect::studioQuality?6720:3360;
    result.modelDelay=static_cast<int>(std::lround(double(delay48)*hostRate/48000));
    const auto lookahead=[](int source,int destination) {
        return source==destination?0:static_cast<int>(std::ceil(32./std::min(1.,double(destination)/source)));
    };
    result.latency=result.modelDelay+static_cast<int>(std::ceil(double(result.inputFrames)*hostRate/result.inputRate))+
        lookahead(hostRate,result.inputRate)+
        static_cast<int>(std::ceil(double(lookahead(result.outputRate,hostRate))*hostRate/result.outputRate))+2;
    return result;
}
}
