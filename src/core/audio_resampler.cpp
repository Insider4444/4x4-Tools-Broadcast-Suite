#include "audio_pipeline.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numeric>

namespace fourbyfour {
AudioResampler::AudioResampler(int source,int destination,int count):channels(count),sourceRate(source),destinationRate(destination) {
    if(count<1||count>2||source<8000||source>192000||destination<8000||destination>192000) throw std::invalid_argument("Unsupported audio sample rate or channel count.");
    cutoff=std::min(1.,double(destination)/source);
    radius=source==destination?0:static_cast<int>(std::ceil(32./cutoff));
    const int countPhases=destination/std::gcd(source,destination);
    // Common audio rates have a small rational phase cycle (e.g. 160 at
    // 44.1->48 kHz). Cache the same band-limited filter instead of computing
    // hundreds of sin/cos functions for every output sample.
    if(radius&&countPhases<=1024&&static_cast<std::size_t>(countPhases)*(radius*2+1)<=1048576) {
        phases.resize(countPhases);
        constexpr double pi=3.14159265358979323846;
        for(int p=0;p<countPhases;++p) {
            const double fraction=double((std::int64_t(p)*source)%destination)/destination;
            auto& phase=phases[p];phase.first=static_cast<int>(std::ceil(fraction-radius));
            const int last=static_cast<int>(std::floor(fraction+radius));double total=0;
            for(int i=phase.first;i<=last;++i) {
                const double d=fraction-i,z=d*cutoff;
                const double sinc=std::abs(z)<1.e-12?1.:std::sin(pi*z)/(pi*z);
                const double window=.42+.5*std::cos(pi*d/radius)+.08*std::cos(2*pi*d/radius);
                phase.weights.push_back(sinc*window*cutoff);total+=phase.weights.back();
            }
            for(auto& value:phase.weights)value/=total;
        }
    }
}
void AudioResampler::reset() { history.clear();ready.clear();base=received=generated=0; }
void AudioResampler::push(const std::array<float,2>& frame) {
    if(!radius) { ready.push_back(frame);return; }
    history.push_back(frame); ++received;
    constexpr double pi=3.14159265358979323846;
    for(;;) {
        if(!phases.empty()) {
            const auto center=generated*sourceRate/destinationRate;
            const auto& phase=phases[static_cast<std::size_t>(generated%phases.size())];
            const auto start=center+phase.first,end=start+static_cast<std::int64_t>(phase.weights.size())-1;
            if(end>=received)break;
            std::array<double,2> value{};
            for(std::size_t tap=0;tap<phase.weights.size();++tap) {
                const auto i=start+static_cast<std::int64_t>(tap);
                if(i>=base&&i>=0&&i<received) for(int c=0;c<channels;++c)value[c]+=history[static_cast<std::size_t>(i-base)][c]*phase.weights[tap];
            }
            ready.push_back({static_cast<float>(value[0]),static_cast<float>(value[1])});++generated;
            const auto keep=generated*sourceRate/destinationRate-radius-1;
            while(base<keep&&!history.empty()){history.pop_front();++base;}
            continue;
        }
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
