#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <iostream>

#include "envs_lfos.h"

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
        double pan = 0.0f;			      // -1.0f for full left, +1.0f for full right, 0.0f for center
        EnvParams vol_env;                // Volume envelope
        EnvParams mod_env;                // Modulator envelope
        LfoParams vib_lfo;                // Volume LFO
        LfoParams mod_lfo;                // Modulator LFO
        LowPassFilter filter;             // Filter base settings
        double mod_env_to_pitch = 0;       // The max sample pitch shift in cents that the modulator envelope will apply
        double mod_env_to_filter = 0;      // The max filter frequency pitch shift in cents that the modulator envelope will apply
        double mod_lfo_to_pitch = 0;       // The max sample pitch shift in cents that the modulator lfo will apply
        double mod_lfo_to_filter = 0;      // The max filter frequency pitch shift in cents that the modulator lfo will apply
        double mod_lfo_to_volume = 0;      // The max radius in dB that the modulator lfo will make the volume rise and fall
        double vib_lfo_to_pitch = 0;       // The max sample pitch shift in cents that the modulator lfo will apply
        double key_to_vol_env_hold = 0.0f; // Envelope Hold change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        double key_to_vol_env_decay = 0.0f;// Envelope Decay change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        double key_to_mod_env_hold = 0.0f; // Envelope Hold change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        double key_to_mod_env_decay = 0.0f;// Envelope Decay change between an octave (0.0 = no change per key, 100.0 twice as long for key-1)
        double scale_tuning = 1.0f;	      // Difference in semitones between each MIDI note
        double tuning = 0.0f;		      // Combination of the sf2 coarse and fine tuning, could be added to MIDI key directly to get corrected pitch
        double init_attenuation = 0.0f;    // Value to subtract from note volume in cB
        char name[24]{ 0 };
    };

    struct PresetIndex {
        u8 bank;
        u8 program;
    };

    struct Preset {
        std::string name;
        std::vector<Zone> zones;
    };

    struct ChunkId {
        union {
            u32 id = 0;
            char id_chr[4];
        };
        static u32 from_string(const char* v)
        {
            return *reinterpret_cast<const uint32_t*>(v);
        }

        [[nodiscard]] std::shared_ptr<char> c_str() const;
        bool verify(const char* other_id) const;
        bool operator==(const ChunkId& rhs) const { return id == rhs.id; }
        bool operator!=(const ChunkId& rhs) const { return id == rhs.id; }
        bool operator==(const char* rhs) const { return *reinterpret_cast<const uint32_t*>(rhs) == id; }
        bool operator!=(const char* rhs) const { return *reinterpret_cast<const uint32_t*>(rhs) != id; }
    };

    struct ChunkDataHandler {
        u8* data_pointer = nullptr;
        u32 chunk_bytes_left = 0;
        u8* original_pointer = nullptr;
        ~ChunkDataHandler() {
            if (original_pointer)
                free(original_pointer);
        }
        bool from_buffer(u8* buffer_to_use, u32 size);
        bool from_file(FILE* file, u32 size);
        bool from_ifstream(std::ifstream& file, u32 size, bool free_on_destruction = true);
        bool from_data_handler(const ChunkDataHandler& data, u32 size);
        bool get_data(void* destination, u32 byte_count);
    };

    struct Chunk {
        ChunkId id;
        u32 size = 0;
        bool from_file(FILE*& file) {
            return fread_s(this, sizeof(*this), sizeof(*this), 1, file) > 0;
        }

        bool from_chunk_data_handler(ChunkDataHandler& chunk_data_handler) {
            return chunk_data_handler.get_data(this, sizeof(*this));
        }

        bool verify(const char* other_id) const
        {
            return id.verify(other_id);
        }
    };
#pragma pack(push, 1)
    struct SfVersionTag {
        u16 major;
        u16 minor;
    };

    struct SfPresetHeader {
        char preset_name[20]; // Preset name
        u16 program; // MIDI program number
        u16 bank; // MIDI bank number
        u16 pbag_index; // PBAG index - if not equal to PHDR index, the .sf2 file is invalid
        u32 library;
        u32 genre;
        u32 morphology;
    };

    struct SfBag {
        u16 generator_index;
        u16 modulator_index;
    };

    struct SfModulator {
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
        SfModulator src_oper;
        SFGenerator dest_oper;
        i16 amount;
        SfModulator amount_src_oper;
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
        SfPresetHeader* preset_headers = nullptr;   unsigned int n_preset_headers = 0;
        SfBag* preset_bags = nullptr;               unsigned int n_preset_bags = 0;
        sfModList* preset_mods = nullptr;           unsigned int n_preset_mods = 0;
        sfGenList* preset_gens = nullptr;           unsigned int n_preset_gens = 0;
        sfInst* instruments = nullptr;              unsigned int n_instruments = 0;
        SfBag* instr_bags = nullptr;                unsigned int n_instr_bags = 0;
        sfModList* instr_mods = nullptr;            unsigned int n_instr_mods = 0;
        sfGenList* instr_gens = nullptr;            unsigned int n_instr_gens = 0;
        sfSample* sample_headers = nullptr;         unsigned int n_samples = 0;
    };

    struct RiffChunk {
        ChunkId type{};
        u32 size = 0;
        ChunkId name{};
        void from_file(FILE* file) {
            fread_s(this, sizeof(*this), sizeof(*this), 1, file);
        }
    };

    struct DlsInsh {
        u32 region_count;
        u32 bank_id;
        u32 instr_id;
    };

    enum dlsArtSrc : u16 {
        CONN_SRC_NONE = 0x0000,
        CONN_SRC_LFO = 0x0001,
        CONN_SRC_KEYONVELOCITY = 0x0002,
        CONN_SRC_KEYNUMBER = 0x0003,
        CONN_SRC_EG1 = 0x0004,
        CONN_SRC_EG2 = 0x0005,
        CONN_SRC_PITCHWHEEL = 0x0006,
        CONN_SRC_POLYPRESSURE = 0x0007,
        CONN_SRC_CHANNELPRESSURE = 0x0008,
        CONN_SRC_VIBRATO = 0x0009,
        CONN_SRC_MONOPRESSURE = 0x000A,
        CONN_SRC_CC1 = 0x0081,
        CONN_SRC_CC7 = 0x0087,
        CONN_SRC_CC10 = 0x008A,
        CONN_SRC_CC11 = 0x008B,
        CONN_SRC_CC91 = 0x00DB,
        CONN_SRC_CC93 = 0x00DD,
        CONN_SRC_RPN0 = 0x0100,
        CONN_SRC_RPN1 = 0x0101,
        CONN_SRC_RPN2 = 0x0102,
    };

    enum dlsArtDst : u16 {
        CONN_DST_NONE = 0x0000,
        CONN_DST_GAIN = 0x0001,
        CONN_DST_PITCH = 0x0003,
        CONN_DST_PAN = 0x0004,
        CONN_DST_KEYNUMBER = 0x0005,
        CONN_DST_LEFT = 0x0010,
        CONN_DST_RIGHT = 0x0011,
        CONN_DST_CENTER = 0x0012,
        CONN_DST_LEFTREAR = 0x0013,
        CONN_DST_RIGHTREAR = 0x0014,
        CONN_DST_LFE_CHANNEL = 0x0015,
        CONN_DST_CHORUS = 0x0080,
        CONN_DST_REVERB = 0x0081,
        CONN_DST_LFO_FREQUENCY = 0x0104,
        CONN_DST_LFO_STARTDELAY = 0x0105,
        CONN_DST_VIB_FREQUENCY = 0x0114,
        CONN_DST_VIB_STARTDELAY = 0x0115,
        CONN_DST_EG1_ATTACKTIME = 0x0206,
        CONN_DST_EG1_DECAYTIME = 0x0207,
        CONN_DST_EG1_RELEASETIME = 0x0209,
        CONN_DST_EG1_SUSTAINLEVEL = 0x020A,
        CONN_DST_EG1_DELAYTIME = 0x020B,
        CONN_DST_EG1_HOLDTIME = 0x020C,
        CONN_DST_EG1_SHUTDOWNTIME = 0x020D,
        CONN_DST_EG2_ATTACKTIME = 0x030A,
        CONN_DST_EG2_DECAYTIME = 0x030B,
        CONN_DST_EG2_RELEASETIME = 0x030D,
        CONN_DST_EG2_SUSTAINLEVEL = 0x030E,
        CONN_DST_EG2_DELAYTIME = 0x030F,
        CONN_DST_EG2_HOLDTIME = 0x0310,
        CONN_DST_FILTER_CUTOFF = 0x0500,
        CONN_DST_FILTER_Q = 0x0501,
    };

    enum dlsArtTrn : u16 {
        CONN_TRN_NONE = 0x0000,
        CONN_TRN_CONCAVE = 0x0001,
        CONN_TRN_CONVEX = 0x0002,
        CONN_TRN_SWITCH = 0x0003,
    };

    // Get region header
    struct dlsRgnh{
        u16 key_low;
        u16 key_high;
        u16 vel_low;
        u16 vel_high;
        u16 options; //ignored
        u16 key_group; //ignored
        u16 unknown;
    };

    struct dlsWsmp {
        u32 struct_size = 0;
        u16 root_key = 60;
        i16 fine_tune = 0;
        i32 attenuation = 0;
        u32 options = 0; //ignored
        u32 loop_mode = 0;
        // loop parts, some wsmp chunks dont have it
        u32 wsloop_size = 0; //should be 16 if there's a loop chunk
        u32 loop_type = 0; // always forward loop
        u32 loop_start = 0; // absolute offset in data chunk
        u32 loop_length = 0;
    };

    struct dlsWlnk{
        u16 options; //ignore
        u16 phase_group; //ignore
        u32 channel; //level 1 dls doesnt support stereo, ignore
        u32 smpl_idx;
    };

}
#pragma pack(pop)