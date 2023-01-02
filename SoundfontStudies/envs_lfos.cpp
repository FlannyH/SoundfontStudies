#include "envs_lfos.h"

#include <algorithm>
#include <corecrt_math.h>

namespace Flan {
    void EnvState::update(const EnvParams& env_params, const double dt, const bool correct_attack_phase)
    {
        // Update volume envelope - the idea is that the adsr_timer goes from 1.0 - 0.0 for every state
        switch (static_cast<EnvStage>(stage)) {
        case delay:
            stage = std::min(static_cast<double>(attack), stage + env_params.delay * dt);
            value = -100.0;
            break;
        case attack:
            stage = std::min(static_cast<double>(hold), stage + env_params.attack * dt);
            if (correct_attack_phase)
                value = 6 * log2(stage - static_cast<double>(attack));
            else
                value = -100.0 + 100 * (stage - static_cast<double>(attack));
            break;
        case hold:
            stage = std::min(static_cast<double>(decay), stage + env_params.hold * dt);
            value = 0.0;
            break;
        case decay:
            value -= env_params.decay * dt;
            if (value < env_params.sustain) {
                value = env_params.sustain;
                stage = static_cast<double>(sustain);
            }
            break;
        case sustain:
            // This stage will stay until note_off
            stage = static_cast<double>(sustain);
            value = env_params.sustain;
            break;
        case release:
            // This stage will continue until the volume is 0.0f
            stage = static_cast<double>(release);
            value -= env_params.release * dt;
            if (value <= -100.0) {
                value = -100.0;
                stage = static_cast<double>(off);
            }
            break;
        default:
            break;
        }
    }

    void LfoState::update(const LfoParams& lfo_params, const double dt)
    {
        // Step time
        time += dt;

        // Handle delay
        if (time < lfo_params.delay) {
            state = 0.0;
        }
        else {
            state = sin((time - lfo_params.delay) * lfo_params.freq * 2.0 * 3.141592653589793);
        }
    }

    void LowPassFilter::update(double dt, float& input_l, float& input_r)
    {
        const float feedback = (resonance + resonance / (1.0f - cutoff));
        const float two_pi_fc = 2.0f * 3.141592653589f * cutoff * static_cast<float>(dt);
        const float a = two_pi_fc / (two_pi_fc + 1);

        // Left channel
        state1[0] += a * (input_l - state1[0] + feedback * (state1[0] - state2[0]));
        state2[0] += a * (state1[0] - state2[0]);
        input_l = state2[0];

        // Right channel
        state1[1] += a * (input_r - state1[1] + feedback * (state1[1] - state2[1]));
        state2[1] += a * (state1[1] - state2[1]);
        input_r = state2[1];


        state1[0] = std::clamp(state1[0], -50.0f, +50.0f);
        state1[1] = std::clamp(state1[1], -50.0f, +50.0f);
        state2[0] = std::clamp(state2[0], -50.0f, +50.0f);
        state2[1] = std::clamp(state2[1], -50.0f, +50.0f);
    }
}