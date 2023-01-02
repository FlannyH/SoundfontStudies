#pragma once
#include "common.h"

namespace Flan {
    struct EnvParams {
        double delay = 1000.0;      // Delay in 1.0 / seconds
        double attack = 1000.0;	    // Attack in 1.0 / seconds
        double hold = 1000.0;	    //Hold in 1.0 / seconds
        double decay = 0.0;		    // Decay in dB per second
        double sustain = 0.0;	    //Sustain volume in dB (where a value of -6 dB means 0.5x volume, and -12 dB means 0.25x volume)
        double release = 1000.0;    // Release in dB per second
    };

    enum EnvStage : u8
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
        double stage = delay;    // Current stage in ADSR. If floored to an integer and casted to ADSRstage, you get the actual ADSRstage as an enum
        double value = 0.0;    // Current envelope volume value in dB
        void update(const EnvParams& env_params, double dt, bool correct_attack_phase);
    };

    struct LfoParams {
        double freq = 8.2; // Frequency in Hz
        double delay = 0.0; // Delay in seconds
    };

    struct LfoState {
        double time = 0.0;
        double state = 0.0;
        void update(const LfoParams& lfo_params, double dt);
    };

    struct LowPassFilter {
        float cutoff = 20005.0;
        float resonance = 0.0;
        float state1[2] = { 0.0, 0.0 };
        float state2[2] = { 0.0, 0.0 };
        void update(double dt, float& input_l, float& input_r);
    };
}