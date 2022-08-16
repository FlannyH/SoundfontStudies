#pragma once
#include <cstdio>
#include <unordered_map>
#include <map>
#include "structs.h"

namespace Flan {
    struct Soundfont {
    public:
        Soundfont(std::string path) { from_file(path); }
        Soundfont(){}
        ~Soundfont() { clear(); }
        std::map<u16, Preset> presets;
        std::vector<Sample> samples;
        bool from_file(std::string path);
        bool from_sf2(std::string path);
        bool from_dls(std::string path);
        void clear();
    private:
        void handle_dls_instr(const uint32_t& n_instruments, Flan::ChunkDataHandler& dls_file, std::map<u32, Sample>& _samples);
        void handle_dls_lrgn(Flan::ChunkDataHandler& dls_file, struct dlsInsh& insh, std::map<uint32_t, Flan::Sample>& samples, Flan::Preset& preset);
        void handle_art1(Flan::ChunkDataHandler& dls_file, Zone& zone);
        Preset get_sf2_preset_from_index(size_t index, RawSoundfontData& raw_sf);
        void init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values);
        i16* sample_data = nullptr;
    };
}