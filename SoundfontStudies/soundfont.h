#pragma once
#include <cstdio>
#include <unordered_map>
#include <map>
#include "structs.h"

namespace Flan {
    struct Soundfont {
    public:
        std::map<u16, Preset> presets;
        std::map<int, Sample> samples;
        bool from_file(std::string path);
        Preset get_preset_from_index(size_t index);
        void init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values);
        void clear();
    private:
        sfPresetHeader* preset_headers = nullptr;			int n_preset_headers = 0;
        sfBag* preset_bags = nullptr;			int n_preset_bags = 0;
        sfModList* preset_mods = nullptr;			int n_preset_mods = 0;
        sfGenList* preset_gens = nullptr;			int n_preset_gens = 0;
        sfInst* instruments = nullptr;			int n_instruments = 0;
        sfBag* instr_bags = nullptr;			int n_instr_bags = 0;
        sfModList* instr_mods = nullptr;			int n_instr_mods = 0;
        sfGenList* instr_gens = nullptr;			int n_instr_gens = 0;
        sfSample* sample_headers = nullptr;			int n_samples = 0;
    };
}