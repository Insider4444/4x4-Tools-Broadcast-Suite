#include "audio_level.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace fourbyfour {
AudioLevelMatch::AudioLevelMatch(int rate) {
    if(rate<8000||rate>192000)throw std::invalid_argument("Invalid level matching rate.");
    energy=1.f-std::exp(-1.f/(rate*.15f));
    attack=1.f-std::exp(-1.f/(rate*.02f));
    release=1.f-std::exp(-1.f/(rate*.4f));
}
void AudioLevelMatch::reset(){dryPower=wetPower=0;current=1;}
float AudioLevelMatch::process(float dry,float wet,bool enabled) {
    dryPower+=(std::max(0.f,dry)-dryPower)*energy;
    wetPower+=(std::max(0.f,wet)-wetPower)*energy;
    float target=1;
    // Hold during silence; never raise model noise to match a silent source.
    if(enabled&&wetPower>1.e-10)target=static_cast<float>(std::clamp(std::sqrt(dryPower/wetPower),.125,1.));
    current+=(target-current)*(target<current?attack:release);
    return current;
}
}
