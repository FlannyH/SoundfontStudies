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
        void clear();
    private:
        Preset get_preset_from_index(size_t index, RawSoundfontData& raw_sf);
        void init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values);
        i16* sample_data = nullptr;
    };
}