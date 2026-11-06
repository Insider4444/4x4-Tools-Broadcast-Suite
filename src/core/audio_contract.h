#pragma once
#include "nvidia/audio_effects.h"

namespace fourbyfour {
// The shipped R22/3.0.0 models have a fixed I/O contract. Hosts must be able to
// query it and arrange latency compensation without opening CUDA or a model.
struct AudioContract {
    int inputRate{},outputRate{},inputFrames{},outputFrames{};
    int modelDelay{},latency{};
};
AudioContract audioContract(AudioEffect effect,int hostRate);
}
