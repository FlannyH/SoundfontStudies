#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <algorithm>

namespace Flan {
    enum SFSampleLink : u16 {
        monoSample = 1,
        rightSample = 2,
        leftSample = 4,
        linkedSample = 8,
        RomMonoSample = 0x8001,
        RomRightSample = 0x8002,
        RomLeftSample = 0x8004,
        RomLinkedSample = 0x8008
    };

    struct Sample {
        i16* data;                        // Pointer to the sample data - should always be a valid pointer
        i16* linked;                      // Pointer to linked sample data - only used if sample link type is not monoSample
        float base_sample_rate;           // In samples per second
        u32 length;                       // In samples
        u32 loop_start;                   // In samples
        u32 loop_end;                     // In samples
        Flan::SFSampleLink type;          // Sample link type
    };

    struct EnvParams {
        float delay = 1000.0f;             // Delay in 1.0 / seconds
        float attack = 1000.0f;	          // Attack in 1.0 / seconds
        float hold = 1000.0f;		      // Hold in 1.0 / seconds
        float decay = 0.0f;		      // Decay in dB per second
        float sustain = 0.0f;		      // Sustain volume in dB (where a value of -6 dB means 0.5x volume, and -12 dB means 0.25x volume)
        float release = 1000.0f;		      // Release in dB per second
    };

    enum EnvStage : u8
    {
        Delay = 0,// Hold volume at 0.0
        Attack,   // Slowly rise from 0.0 to 1.0
        Hold,     // Hold at 1.0
        Decay,    // Decay from 1.0 to Sustain
        Sustain,  // Hold at Sustain until note_off
        Release,  // Decay from Sustain to 0.0
        Off,      // Oscillator is not playing, meaning the oscillator is free
    };

    struct EnvState {
        float stage = (float)Delay;    // Current stage in ADSR. If floored to an integer and casted to ADSRstage, you get the actual ADSRstage as an enum
        float value = 0.0f;    // Current envelope volume value in dB
        void Update(EnvParams& env_params, float dt, bool correct_attack_phase) {
            // Get ADSR stage as enum
            EnvStage stage_enum = (EnvStage)((u8)stage);

            // Update volume envelope - the idea is that the adsr_timer goes from 1.0 - 0.0 for every state
            switch (stage_enum) {
            case Delay:
                stage = std::min((float)Attack, stage + env_params.delay * dt);
                value = -100.0f;
                break;
            case Attack:
                stage = std::min((float)Hold, stage + env_params.attack * dt);
                if (correct_attack_phase)
                    value = 6 * log2f(stage - (float)Attack);
                else
                    value = -100.0f + 100 * (stage - (float)Attack);
                break;
            case Hold:
                stage = std::min((float)Decay, stage + env_params.hold * dt);
                value = 0.0f;
                break;
            case Decay:
                value -= env_params.decay * dt;
                if (value < env_params.sustain) {
                    value = env_params.sustain;
                    stage = (float)(Sustain);
                }
                break;
            case Sustain:
                // This stage will stay until note_off
                stage = (float)(Sustain);
                value = env_params.sustain;
                break;
            case Release:
                // This stage will continue until the volume is 0.0f
                stage = (float)(Release);
                value -= env_params.release * dt;
                if (value <= -100.0f) {
                    value = -100.0f;
                    stage = (float)Off;
                }
                break;
            }
        }
    };

    struct LfoParams {
        float freq = 8.2f; // Frequency in Hz
        float delay = 0.0f; // Delay in seconds
    };

    struct LfoState {
        float time = 0.0;
        float state = 0.0f;
        void Update(LfoParams& lfo_params, float dt) {
            // Step time
            time += dt;

            // Handle delay
            if (time < lfo_params.delay) {
                state = 0.0f;
            }
            else {
                state = sinf((time - lfo_params.delay) * lfo_params.freq * 2.0f * 3.14159265359f);
            }
        }
    };

    struct LowPassFilter {
        float cutoff = 20005.f;
        float resonance = 0.0f;
        float state1[2] = { 0.0f, 0.0f };
        float state2[2] = { 0.0f, 0.0f };
        void Update(float dt, float& input_l, float& input_r) {
            const float feedback = (resonance + resonance / (1.0f - cutoff));
            const float TWO_PI_FC = 2.0f * 3.141592653589f * cutoff * dt;
            float a = TWO_PI_FC / (TWO_PI_FC + 1);

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
    };

    struct Zone {
        u8 key_range_low = 0;		      // Lowest MIDI key in this zone
        u8 key_range_high = 127;	      // Highest MIDI key in this zone
        u8 vel_range_low = 0;		      // Lowest possible velocity in this zone
        u8 vel_range_high = 0;		      // Highest possible velocity in this zone
        u32 sample_index = 0;		      // Which sample in the Soundfont::samples array the zone uses
        i32 sample_start_offset = 0;      // Apparently this is a thing, instrument zones can offset
        i32 sample_end_offset = 0;        // the sample start, end, loop start, and loop end with a
        i32 sample_loop_start_offset = 0; // generator setting, wack. Add these to the corresponding
        i32 sample_loop_end_offset = 0;   // variables in the sample struct during runtime.
        i32 root_key_offset = 0;          // Root key relative to sample's root key
        bool loop_enable = false;	      // True if sample loops, False if sample does not loop
        u8 key_override = 255;            // If value is below 128, this zone always plays as if it were triggered by this MIDI key.
        u8 vel_override = 255;            // If value is below 128, this zone always plays as if it were triggered with this velocity.
        float pan = 0.0f;			      // -1.0f for full left, +1.0f for full right, 0.0f for center
        EnvParams vol_env;                // Volume envelope
        EnvParams mod_env;                // Modulator envelope
        LfoParams vib_lfo;                // Volume LFO
        LfoParams mod_lfo;                // Modulator LFO
        LowPassFilter filter;             // Filter base settings
        float mod_env_to_pitch = 0;       // The max sample pitch shift in cents that the modulator envelope will apply
        float mod_env_to_filter = 0;      // The max filter frequency pitch shift in cents that the modulator envelope will apply
        float mod_lfo_to_pitch = 0;       // The max sample pitch shift in cents that the modulator lfo will apply
        float mod_lfo_to_filter = 0;      // The max filter frequency pitch shift in cents that the modulator lfo will apply
        float mod_lfo_to_volume = 0;      // The max radius in dB that the modulator lfo will make the volume rise and fall
        float vib_lfo_to_pitch = 0;       // The max sample pitch shift in cents that the modulator lfo will apply
        float key_to_vol_env_hold = 0.0f; // Envelope Hold change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        float key_to_vol_env_decay = 0.0f;// Envelope Decay change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        float key_to_mod_env_hold = 0.0f; // Envelope Hold change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        float key_to_mod_env_decay = 0.0f;// Envelope Decay change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        float scale_tuning = 1.0f;	      // Difference in semitones between each MIDI note
        float tuning = 0.0f;		      // Combination of the sf2 coarse and fine tuning, could be added to MIDI key directly to get corrected pitch
        float init_attenuation = 0.0f;    // Value to subtract from note volume in cB
    };

    struct PresetIndex {
        u8 bank;
        u8 program;
    };

    struct Preset {
        std::string name;
        std::vector<Zone> zones;
    };

    struct ChunkID {
        union {
            u32 id = 0;
            char id_chr[4];
        };
        static constexpr u32 from_string(const char* v) {
            return *(u32*)v;
        }
        bool verify(const char* other_id) {
            if (*this != other_id) {
                printf("[ERROR] Chunk ID mismatch! Expected '%s'\n", other_id);
            }
            return *this == other_id;
        }
        bool operator==(const ChunkID& rhs) { return id == rhs.id; }
        bool operator!=(const ChunkID& rhs) { return id == rhs.id; }
        bool operator==(const char* rhs) { return *(u32*)rhs == id; }
        bool operator!=(const char* rhs) { return *(u32*)rhs != id; }
    };

    struct ChunkDataHandler {
        u8* data_pointer = nullptr;
        u32 chunk_bytes_left = 0;
        u8* original_pointer = nullptr;
        ~ChunkDataHandler() {
            free(original_pointer);
        }
        bool from_buffer(u8* buffer_to_use, u32 size) {
            if (buffer_to_use == nullptr)
                return false;
            data_pointer = buffer_to_use;
            chunk_bytes_left = size;
            return true;
        }

        bool from_file(FILE* file, u32 size) {
            if (size == 0) {
                return false;
            }
            original_pointer = (u8*)malloc(size);
            data_pointer = original_pointer;
            if (data_pointer == 0) { return false; }
            fread_s(data_pointer, size, size, 1, file);
            chunk_bytes_left = size;
            return true;
        }

        bool get_data(void* destination, u32 byte_count) {
            if (chunk_bytes_left >= byte_count) {
                if (destination != nullptr) { // Sending a nullptr is valid, it just discards byte_count number of bytes
                    memcpy(destination, data_pointer, byte_count);
                }
                chunk_bytes_left -= byte_count;
                data_pointer += byte_count;
                return true;
            }
            return false;
        }
    };

    struct Chunk {
        ChunkID id;
        u32 size = 0;
        bool from_file(FILE*& file) {
            return fread_s(this, sizeof(*this), sizeof(*this), 1, file) > 0;
        }

        bool from_chunk_data_handler(ChunkDataHandler& chunk_data_handler) {
            return chunk_data_handler.get_data(this, sizeof(*this));
        }

        bool verify(const char* other_id) {
            return id.verify(other_id);
        }
    };
#pragma pack(push, 1)
    struct sfVersionTag {
        u16 major;
        u16 minor;
    };

    struct sfPresetHeader {
        char preset_name[20]; // Preset name
        u16 program; // MIDI program number
        u16 bank; // MIDI bank number
        u16 pbag_index; // PBAG index - if not equal to PHDR index, the .sf2 file is invalid
        u32 library;
        u32 genre;
        u32 morphology;
    };

    struct sfBag {
        u16 generator_index;
        u16 modulator_index;
    };

    struct SFModulator {
        u16 index : 7;
        u16 cc_flag : 1;
        u16 direction : 1;
        u16 polarity : 1;
        u16 type : 6;
    };

    struct RangesType {
        u8 low;
        u8 high;
    };

    union GenAmountType {
        RangesType ranges;
        u16 u_amount;
        i16 s_amount;
    };

    enum SFGenerator : u16 {
        startAddrsOffset = 0,
        endAddrsOffset,
        startloopAddrsOffset,
        endloopAddrsOffset,
        startAddrsCoarseOffset,
        modLfoToPitch,
        vibLfoToPitch,
        modEnvToPitch,
        initialFilterFc,
        initialFilterQ,
        modLfoToFilterFc,
        modEnvToFilterFc,
        endAddrsCoarseOffset,
        modLfoToVolume,
        unused1,
        chorusEffectsSend,
        reverbEffectsSend,
        pan,
        unused2,
        unused3,
        unused4,
        delayModLFO,
        freqModLFO,
        delayVibLFO,
        freqVibLFO,
        delayModEnv,
        attackModEnv,
        holdModEnv,
        decayModEnv,
        sustainModEnv,
        releaseModEnv,
        keynumToModEnvHold,
        keynumToModEnvDecay,
        delayVolEnv,
        attackVolEnv,
        holdVolEnv,
        decayVolEnv,
        sustainVolEnv,
        releaseVolEnv,
        keynumToVolEnvHold,
        keynumToVolEnvDecay,
        instrument,
        reserved1,
        keyRange,
        velRange,
        startloopAddrsCoarseOffset,
        keynum,
        velocity,
        initialAttenuation,
        reserved2,
        endloopAddrsCoarseOffset,
        coarseTune,
        fineTune,
        sampleID,
        sampleModes,
        reserved3,
        scaleTuning,
        exclusiveClass,
        overridingRootKey,
        unused5,
        endOper
    };

    static std::string SFGenerator_names[]{
        "startAddrsOffset",
        "endAddrsOffset",
        "startloopAddrsOffset",
        "endloopAddrsOffset",
        "startAddrsCoarseOffset",
        "modLfoToPitch",
        "vibLfoToPitch",
        "modEnvToPitch",
        "initialFilterFc",
        "initialFilterQ",
        "modLfoToFilterFc",
        "modEnvToFilterFc",
        "endAddrsCoarseOffset",
        "modLfoToVolume",
        "unused1",
        "chorusEffectsSend",
        "reverbEffectsSend",
        "pan",
        "unused2",
        "unused3",
        "unused4",
        "delayModLFO",
        "freqModLFO",
        "delayVibLFO",
        "freqVibLFO",
        "delayModEnv",
        "attackModEnv",
        "holdModEnv",
        "decayModEnv",
        "sustainModEnv",
        "releaseModEnv",
        "keynumToModEnvHold",
        "keynumToModEnvDecay",
        "delayVolEnv",
        "attackVolEnv",
        "holdVolEnv",
        "decayVolEnv",
        "sustainVolEnv",
        "releaseVolEnv",
        "keynumToVolEnvHold",
        "keynumToVolEnvDecay",
        "instrument",
        "reserved1",
        "keyRange",
        "velRange",
        "startloopAddrsCoarseOffset",
        "keynum",
        "velocity",
        "initialAttenuation",
        "reserved2",
        "endloopAddrsCoarseOffset",
        "coarseTune",
        "fineTune",
        "sampleID",
        "sampleModes",
        "reserved3",
        "scaleTuning",
        "exclusiveClass",
        "overridingRootKey",
        "unused5",
        "endOpe",
    };

    enum GenApplyMode :u8 {
        add,
        clamp_range,
    };

    struct GenFlags {
        u8 instr_only : 1;
        GenApplyMode apply_mode : 2;
    };

    static GenFlags gen_flags[59] = {
        GenFlags{true, add}, // 0
        GenFlags{true, add},
        GenFlags{true, add},
        GenFlags{true, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 5
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 10
        GenFlags{false, add},
        GenFlags{true, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 15
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 20
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 25
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 30
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 35
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add}, // 40
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, clamp_range},
        GenFlags{false, clamp_range},
        GenFlags{true, add}, // 45
        GenFlags{true, add},
        GenFlags{true, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{true, add}, // 50
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{false, add},
        GenFlags{true, add},
        GenFlags{false, add}, // 55
        GenFlags{false, add},
        GenFlags{true, add},
        GenFlags{true, add},
    };

    enum SFTransform : u16 {
        linear = 0,
        absolute_value = 2,
    };

    struct sfModList {
        SFModulator src_oper;
        SFGenerator dest_oper;
        i16 amount;
        SFModulator amount_src_oper;
        SFTransform trans_oper;
    };

    struct sfGenList {
        SFGenerator oper;
        GenAmountType amount;
    };
    struct sfInst {
        u8 name[20];
        u16 bag_index;
    };

    struct sfSample {
        char name[20];
        u32 start_index;
        u32 end_index;
        u32 loop_start_index;
        u32 loop_end_index;
        u32 sample_rate;
        u8 original_key;
        i8 correction;
        u16 sample_link;
        SFSampleLink type;
    };

    struct RawSoundfontData {
        sfPresetHeader* preset_headers = nullptr;   int n_preset_headers = 0;
        sfBag* preset_bags = nullptr;               int n_preset_bags = 0;
        sfModList* preset_mods = nullptr;           int n_preset_mods = 0;
        sfGenList* preset_gens = nullptr;           int n_preset_gens = 0;
        sfInst* instruments = nullptr;              int n_instruments = 0;
        sfBag* instr_bags = nullptr;                int n_instr_bags = 0;
        sfModList* instr_mods = nullptr;            int n_instr_mods = 0;
        sfGenList* instr_gens = nullptr;            int n_instr_gens = 0;
        sfSample* sample_headers = nullptr;         int n_samples = 0;
    };

    struct RiffChunk {
        ChunkID type;
        u32 size;
        ChunkID name;
        void from_file(FILE* file) {
            fread_s(this, sizeof(*this), sizeof(*this), 1, file);
        }
    };

    struct dlsInsh {
        u32 region_count;
        u32 bank_id;
        u32 instr_id;
    };

    enum dlsArticulatorDefines : u16 {
        // Generic Sources,
        CONN_SRC_NONE = 0x0000,
        CONN_SRC_LFO = 0x0001,
        CONN_SRC_KEYONVELOCITY = 0x0002,
        CONN_SRC_KEYNUMBER = 0x0003,
        CONN_SRC_EG1 = 0x0004,
        CONN_SRC_EG2 = 0x0005,
        CONN_SRC_PITCHWHEEL = 0x0006,
        // Midi Controllers 0-127,
        CONN_SRC_CC1 = 0x0081,
        CONN_SRC_CC7 = 0x0087,
        CONN_SRC_CC10 = 0x008a,
        CONN_SRC_CC11 = 0x008b,
        // Generic Destinations,
        CONN_DST_NONE = 0x0000,
        CONN_DST_ATTENUATION = 0x0001,
        CONN_DST_RESERVED = 0x0002,
        CONN_DST_PITCH = 0x0003,
        CONN_DST_PAN = 0x0004,
        // LFO Destinations,
        CONN_DST_LFO_FREQUENCY = 0x0104,
        CONN_DST_LFO_STARTDELAY = 0x0105,
        // EG1 Destinations,
        CONN_DST_EG1_ATTACKTIME = 0x0206,
        CONN_DST_EG1_DECAYTIME = 0x0207,
        CONN_DST_EG1_RESERVED = 0x0208,
        CONN_DST_EG1_RELEASETIME = 0x0209,
        CONN_DST_EG1_SUSTAINLEVEL = 0x020a,
        // EG2 Destinations,
        CONN_DST_EG2_ATTACKTIME = 0x030a,
        CONN_DST_EG2_DECAYTIME = 0x030b,
        CONN_DST_EG2_RESERVED = 0x030c,
        CONN_DST_EG2_RELEASETIME = 0x030d,
        CONN_DST_EG2_SUSTAINLEVEL = 0x030e,
        CONN_TRN_NONE = 0x0000,
        CONN_TRN_CONCAVE = 0x0001
    };
}
#pragma pack(pop)