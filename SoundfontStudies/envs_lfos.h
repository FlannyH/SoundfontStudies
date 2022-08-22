#pragma once
#include "common.h"

namespace Flan {
    struct EnvParams {
        float delay = 1000.0f;             // Delay in 1.0 / seconds
        float attack = 1000.0f;	          // Attack in 1.0 / seconds
        float hold = 1000.0f;		      // Hold in 1.0 / seconds
        float decay = 0.0f;		      // Decay in dB per second
        float sustain = 0.0f;		      // Sustain volume in dB (where a value of -6 dB means 0.5x volume, and -12 dB means 0.25x volume)
        float release = 1000.0f;		      // Release in dB per second
    };

    enum envStage : u8
    {
        delay = 0,// Hold volume at 0.0
        attack,   // Slowly rise from 0.0 to 1.0
        hold,     // Hold at 1.0
        decay,    // Decay from 1.0 to Sustain
        sustain,  // Hold at Sustain until note_off
        release,  // Decay from Sustain to 0.0
        off,      // Oscillator is not playing, meaning the oscillator is free
    };

    struct EnvState {
        float stage = static_cast<float>(delay);    // Current stage in ADSR. If floored to an integer and casted to ADSRstage, you get the actual ADSRstage as an enum
        float value = 0.0f;    // Current envelope volume value in dB
        void update(const EnvParams& env_params, float dt, bool correct_attack_phase);
    };

    struct LfoParams {
        float freq = 8.2f; // Frequency in Hz
        float delay = 0.0f; // Delay in seconds
    };

    struct LfoState {
        float time = 0.0;
        float state = 0.0f;
        void update(const LfoParams& lfo_params, float dt);
    };

    struct LowPassFilter {
        float cutoff = 20005.f;
        float resonance = 0.0f;
        float state1[2] = { 0.0f, 0.0f };
        float state2[2] = { 0.0f, 0.0f };
        void update(float dt, float& input_l, float& input_r);
    };
}