#pragma once
#include <map>
#include "structs.h"
#include "riff_tree.h"

namespace Flan {
    struct Soundfont {
    public:
        explicit Soundfont(const std::string& path) { from_file(path); }
        Soundfont() = default;
        ~Soundfont() { clear(); }
        std::map<u16, Preset> presets;
        std::vector<Sample> samples;
        bool from_file(const std::string& path);
        bool from_sf2(const std::string& path);
        bool from_dls(const std::string& path);
        void dls_get_samples(Flan::RiffTree& riff_tree);
        void clear();
    private:
        void handle_art1(Flan::ChunkDataHandler& dls_file, Zone& zone) const;
        Preset get_sf2_preset_from_index(size_t index, RawSoundfontData& raw_sf);
        i16* _sample_data = nullptr;
    };
}