#include "audio_pipeline.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fourbyfour {
AudioResampler::AudioResampler(int source,int destination,int count):channels(count),sourceRate(source),destinationRate(destination) {
    if(count<1||count>2||source<8000||source>192000||destination<8000||destination>192000) throw std::invalid_argument("Unsupported audio sample rate or channel count.");
    cutoff=std::min(1.,double(destination)/source);
    radius=source==destination?0:static_cast<int>(std::ceil(32./cutoff));
}
void AudioResampler::reset() { history.clear();ready.clear();base=received=generated=0; }
void AudioResampler::push(const std::array<float,2>& frame) {
    if(!radius) { ready.push_back(frame);return; }
    history.push_back(frame); ++received;
    constexpr double pi=3.14159265358979323846;
    for(;;) {
        const double center=double(generated)*sourceRate/destinationRate;
        const auto start=static_cast<std::int64_t>(std::ceil(center-radius)),end=static_cast<std::int64_t>(std::floor(center+radius));
        if(end>=received) break;
        std::array<double,2> value{}; double total=0;
        for(auto i=start;i<=end;++i) {
            const double d=center-i,z=d*cutoff;
            const double sinc=std::abs(z)<1.e-12?1.:std::sin(pi*z)/(pi*z);
            const double window=.42+.5*std::cos(pi*d/radius)+.08*std::cos(2*pi*d/radius);
            const double weight=sinc*window*cutoff; total+=weight;
            if(i>=base&&i>=0&&i<received) for(int c=0;c<channels;++c) value[c]+=history[static_cast<std::size_t>(i-base)][c]*weight;
        }
        std::array<float,2> result{};
        for(int c=0;c<channels;++c) result[c]=static_cast<float>(value[c]/total);
        ready.push_back(result); ++generated;
        const auto keep=static_cast<std::int64_t>(std::floor(double(generated)*sourceRate/destinationRate))-radius-1;
        while(base<keep&&!history.empty()) { history.pop_front();++base; }
    }
}
bool AudioResampler::take(std::array<float,2>& frame) { if(ready.empty()) return false;frame=ready.front();ready.pop_front();return true; }
}
