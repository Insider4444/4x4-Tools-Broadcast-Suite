#pragma once
namespace fourbyfour {
// Linked attenuation only: neural resynthesis must not turn a quiet recording
// into a loud one. Output Gain remains the explicit control for amplification.
class AudioLevelMatch {
    double dryPower{},wetPower{};
    float current{1},energy{},attack{},release{};
public:
    explicit AudioLevelMatch(int sampleRate);
    void reset();
    float process(float dryMeanSquare,float wetMeanSquare,bool enabled=true);
};
}
