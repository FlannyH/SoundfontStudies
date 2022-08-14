#include "soundfont.h"
#include "soundfont.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

#define VERBOSE 1
#define PRINT_AT_ALL 0

namespace Flan {
    template<class... Args>
    void print(const char* fmt, Args... args) {
#if PRINT_AT_ALL
        printf(fmt, args...);
#endif
    }
    template<class... Args>
    void print_verbose(const char* fmt, Args... args) {
#if VERBOSE
        print(fmt, args...);
#endif
    }


    bool Soundfont::from_file(std::string path) {
        // We use this for easy data sharing between functions, without exposing this to the end user
        RawSoundfontData raw_sf{};

        // Open file
        FILE* in_file;
        fopen_s(&in_file, path.c_str(), "rb"); // Todo: un-hardcode this
        if (!in_file) { print("[ERROR] Could not open file!\n"); return false; }

        // Read RIFF chunk header
        {
            Chunk riff_chunk;
            riff_chunk.from_file(in_file);
            riff_chunk.verify("RIFF");
        }

        // There should be a 'sfbk' chunk now
        {
            ChunkID sfbk;
            fread_s(&sfbk, sizeof(ChunkID), sizeof(ChunkID), 1, in_file);
            if (sfbk != "sfbk") { print("[ERROR] Expected an 'sfbk' chunk, but did not find one!\n"); return false; }
        }

        print_verbose("---INFO LIST---\n\n");
        // There are 3 LIST chunks. The first one is the INFO list - this has metadata about the soundfont
        {
            // Read the chunk header
            Chunk curr_chunk;
            curr_chunk.from_file(in_file);
            curr_chunk.verify("LIST");

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // INFO chunk header
            ChunkID info;
            curr_chunk_data.get_data(&info, sizeof(ChunkID));
            if (info != "INFO") { print("[ERROR] Expected an 'INFO' chunk, but did not find one!\n"); return false; }


            // Handle all chunks in LIST chunk
            while (1) {
                Chunk chunk;
                bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
                if (!should_continue) { break; }
#if VERBOSE
                if (chunk.id == "ifil") { // Soundfont version
                    sfVersionTag version;
                    curr_chunk_data.get_data(&version, sizeof(version));
                    print_verbose("[INFO] Soundfont version: %i.%i\n", version.major, version.minor);
                }
                else if (chunk.id == "INAM") { // Soundfont name
                    char* soundfont_name = (char*)malloc(chunk.size);
                    curr_chunk_data.get_data(soundfont_name, chunk.size);
                    print_verbose("[INFO] Soundfont name: '%s'\n", soundfont_name);
                    free(soundfont_name);
                }
                else if (chunk.id == "isng") { // Wavetable sound engine name
                    char* wavetable_name = (char*)malloc(chunk.size);
                    curr_chunk_data.get_data(wavetable_name, chunk.size);
                    print_verbose("[INFO] Wavetable engine name: '%s'\n", wavetable_name);
                    free(wavetable_name);
                }
#endif
                else { // Not a chunk we're interested in, skip it
                    curr_chunk_data.get_data(nullptr, chunk.size);
                }
            }
        }

        print_verbose("\n---sdta LIST---\n\n");
        // There are 3 LIST chunks. The second one is the sdta list - contains raw sample data
        {
            // Read the chunk header
            Chunk curr_chunk;
            curr_chunk.from_file(in_file);
            curr_chunk.verify("LIST");

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // INFO chunk header
            ChunkID info;
            curr_chunk_data.get_data(&info, sizeof(ChunkID));
            if (info != "sdta") { print("[ERROR] Expected an 'sdta' chunk, but did not find one!\n"); return false; }

            // Handle all chunks in LIST chunk
            while (1) {
                Chunk chunk;
                bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
                if (!should_continue) { break; }

                if (chunk.id == "smpl") { // Raw sample data
                    sample_data = (i16*)malloc(chunk.size);
                    curr_chunk_data.get_data(sample_data, chunk.size);
                    print_verbose("[INFO] Found sample data, %i bytes total\n", chunk.size);
                }
                else { // Not a chunk we're interested in, skip it (unlikely in this list though)
                    curr_chunk_data.get_data(nullptr, chunk.size);
                }
            }
        }

        print_verbose("\n---pdta LIST---\n\n");
        // There are 3 LIST chunks. The second one is the pdta list - this has presets, instruments, and sample header data
        raw_sf.preset_headers = nullptr;  raw_sf.n_preset_headers = 0;
        raw_sf.preset_bags = nullptr;     raw_sf.n_preset_bags = 0;
        raw_sf.preset_mods = nullptr;     raw_sf.n_preset_mods = 0;
        raw_sf.preset_gens = nullptr;     raw_sf.n_preset_gens = 0;
        raw_sf.instruments = nullptr;     raw_sf.n_instruments = 0;
        raw_sf.instr_bags = nullptr;      raw_sf.n_instr_bags = 0;
        raw_sf.instr_mods = nullptr;      raw_sf.n_instr_mods = 0;
        raw_sf.instr_gens = nullptr;      raw_sf.n_instr_gens = 0;
        raw_sf.sample_headers = nullptr;  raw_sf.n_samples = 0;
        {
            // Read the chunk header
            Chunk curr_chunk;
            curr_chunk.from_file(in_file);
            curr_chunk.verify("LIST");

            // INFO chunk header
            ChunkID info;
            fread_s(&info, sizeof(ChunkID), sizeof(ChunkID), 1, in_file);
            if (info != "pdta") { print("[ERROR] Expected an 'ptda' chunk, but did not find one!\n"); return false; }

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // Handle all chunks in LIST chunk
            while (1) {
                Chunk chunk;
                bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
                if (!should_continue) { break; }

                if (chunk.id == "phdr") { // Preset header
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_headers = (sfPresetHeader*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.preset_headers, chunk.size);
                    raw_sf.n_preset_headers = chunk.size / sizeof(sfPresetHeader);
                    print_verbose("[INFO] Found %zu preset header", chunk.size / sizeof(sfPresetHeader));
                    if (chunk.size / sizeof(sfPresetHeader) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pbag") { // Preset bags
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_bags = (sfBag*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.preset_bags, chunk.size);
                    raw_sf.n_preset_bags = chunk.size / sizeof(sfBag);
                    print_verbose("[INFO] Found %zu preset bag", chunk.size / sizeof(sfBag));
                    if (chunk.size / sizeof(sfBag) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pmod") { // Preset modulators
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_mods = (sfModList*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.preset_mods, chunk.size);
                    raw_sf.n_preset_mods = chunk.size / sizeof(sfModList);
                    print_verbose("[INFO] Found %zu preset modulator", chunk.size / sizeof(sfModList));
                    if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pgen") { // Preset generator
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_gens = (sfGenList*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.preset_gens, chunk.size);
                    raw_sf.n_preset_gens = chunk.size / sizeof(sfGenList);
                    print_verbose("[INFO] Found %zu preset generator", chunk.size / sizeof(sfGenList));
                    if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "inst") { // Instrument header
                    // Allocate enough space and copy the data into it
                    raw_sf.instruments = (sfInst*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.instruments, chunk.size);
                    raw_sf.n_instruments = chunk.size / sizeof(sfInst);
                    print_verbose("[INFO] Found %zu instrument", chunk.size / sizeof(sfInst));
                    if (chunk.size / sizeof(sfInst) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "ibag") { // Instrument bag
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_bags = (sfBag*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.instr_bags, chunk.size);
                    raw_sf.n_instr_bags = chunk.size / sizeof(sfBag);
                    print_verbose("[INFO] Found %zu instrument bag", chunk.size / sizeof(sfBag));
                    if (chunk.size / sizeof(sfBag) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "imod") { // Instrument modulator
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_mods = (sfModList*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.instr_mods, chunk.size);
                    raw_sf.n_instr_mods = chunk.size / sizeof(sfModList);
                    print_verbose("[INFO] Found %zu instrument modulator", chunk.size / sizeof(sfModList));
                    if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "igen") { // Instrument generator
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_gens = (sfGenList*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.instr_gens, chunk.size);
                    raw_sf.n_instr_gens = chunk.size / sizeof(sfGenList);
                    print_verbose("[INFO] Found %zu instrument generator", chunk.size / sizeof(sfGenList));
                    if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "shdr") { // Sample headers
                    // Allocate enough space and copy the data into it
                    raw_sf.sample_headers = (sfSample*)malloc(chunk.size);
                    curr_chunk_data.get_data(raw_sf.sample_headers, chunk.size);
                    raw_sf.n_samples = chunk.size / sizeof(sfSample);
                    print_verbose("[INFO] Found %zu sample", chunk.size / sizeof(sfSample));
                    if (chunk.size / sizeof(sfSample) > 1) { print_verbose("s"); }
                }
                else { // Not a chunk we're interested in, skip it (unlikely in this list though)
                    curr_chunk_data.get_data(nullptr, chunk.size);
                }
                print_verbose("\n");
            }
            print_verbose("");
        }

        print_verbose("\n--SAMPLES--\n\n");
        // Load all the samples into the list
        int x = 0;
        while (1) {
            Sample new_sample;

            // Assume MIDI key number 60 to be the base key
            int correction = 60 - raw_sf.sample_headers[x].original_key; // Note space
            correction *= 100; // Cent space
            correction += raw_sf.sample_headers[x].correction;
            float corr_mul = (float)correction / 1200.0f;
            corr_mul = powf(2.0f, corr_mul);
            new_sample.base_sample_rate = raw_sf.sample_headers[x].sample_rate * corr_mul;

            // Allocate memory and copy the sample data into it
            auto start = raw_sf.sample_headers[x].start_index;
            new_sample.length = raw_sf.sample_headers[x].end_index - start;
            size_t length = new_sample.length * sizeof(i16);
            new_sample.data = (i16*)&sample_data[raw_sf.sample_headers[x].start_index];

            // Loop data
            new_sample.loop_start = raw_sf.sample_headers[x].loop_start_index - start;
            new_sample.loop_end = raw_sf.sample_headers[x].loop_end_index - start;
            new_sample.type = raw_sf.sample_headers[x].type;
            new_sample.linked = (i16*)&sample_data[raw_sf.sample_headers[raw_sf.sample_headers[x].sample_link].start_index];
            print_verbose("\t%s:\n", raw_sf.sample_headers[x].name);
            print_verbose("\tSample rate (before correction): %i\n", raw_sf.sample_headers[x].sample_rate);
            print_verbose("\tSample rate (after correction): %f\n", new_sample.base_sample_rate);
            print_verbose("\tSample data: %i - %i\n", raw_sf.sample_headers[x].start_index, raw_sf.sample_headers[x].end_index);
            print_verbose("\tSample loop: %i - %i\n", raw_sf.sample_headers[x].loop_start_index, raw_sf.sample_headers[x].loop_end_index);
            print_verbose("\tSample type: %i\n", raw_sf.sample_headers[x].type);
            print_verbose("\tSample link: %i\n", raw_sf.sample_headers[x].sample_link);
            print_verbose("\tPitch correction (cents): %i\n", correction);
            print_verbose("\tPitch multiplier (percent): %0.3f%%\n", corr_mul * 100.0);

            // Add to map
            samples.push_back(new_sample);

            x++;
            if (strcmp(raw_sf.sample_headers[x].name, "EOS") == 0) {
                break;
            }
            if (x >= raw_sf.n_samples) {
                break;
            }
        }

        print_verbose("\n--PRESETS--\n\n");
        for (int p_id = 0; p_id < raw_sf.n_preset_headers - 1; p_id++) {
            get_preset_from_index(p_id, raw_sf);
        }

        if (!raw_sf.sample_headers) {
            return false;
        }

        // Close the file
        fclose(in_file);
        print("Soundfont '%s' loaded succesfully!", path.c_str());

        // Free temporary pointers
        void* pointers_to_clear[] = { raw_sf.preset_headers, raw_sf.preset_bags, raw_sf.preset_mods, raw_sf.preset_gens, raw_sf.instruments, raw_sf.instr_bags, raw_sf.instr_mods, raw_sf.instr_gens, raw_sf.sample_headers };
        for (auto pointer : pointers_to_clear)
            free(pointer);

        return true;
    }

    Preset Soundfont::get_preset_from_index(size_t index, RawSoundfontData& raw_sf) {
        // Prepare misc variables
        std::map<std::string, GenAmountType> preset_global_generator_values;
        std::map<std::string, GenAmountType> instrument_global_generator_values;
        Preset final_preset;
        final_preset.name = raw_sf.preset_headers[index].preset_name;

        // Get preset zones from index
        uint16_t preset_zone_start = raw_sf.preset_headers[index].pbag_index;
        uint16_t zone_end = raw_sf.preset_headers[index + 1].pbag_index;

        // Loop over all preset zones
        for (uint16_t preset_zone_index = preset_zone_start; preset_zone_index < zone_end; preset_zone_index++) {
            // Prepare loop variables
            std::map<std::string, GenAmountType> preset_zone_generator_values;
            uint16_t generator_start = raw_sf.preset_bags[preset_zone_index].generator_index;
            uint16_t generator_end = raw_sf.preset_bags[preset_zone_index + 1].generator_index;

            // Loop over all preset zone's generator values
            for (uint16_t generator_index = generator_start; generator_index < generator_end; generator_index++) {
                // Get name and value
                std::string oper_name = SFGenerator_names[raw_sf.preset_gens[generator_index].oper];
                GenAmountType oper_value = raw_sf.preset_gens[generator_index].amount;

                // Insert into zone map
                preset_zone_generator_values[oper_name] = oper_value;
            }

            // Does the instrument ID exist?
            if (preset_zone_generator_values.find("instrument") == preset_zone_generator_values.end()) {
                // If not, this is the global preset zone, save it and go to next preset zone
                preset_global_generator_values = preset_zone_generator_values;
                continue;
            }

            // Get instrument ID
            uint16_t instrument_id = preset_zone_generator_values["instrument"].u_amount;

            // Get instrument zones
            uint16_t instrument_start = raw_sf.instruments[instrument_id].bag_index;
            uint16_t instrument_end = raw_sf.instruments[instrument_id + 1].bag_index;

            // Loop over all instrument zones
            for (uint16_t instrument_index = instrument_start; instrument_index < instrument_end; instrument_index++) {
                // Prepare instrument zone
                std::map<std::string, GenAmountType> instrument_zone_generator_values;
                uint16_t instrument_gen_start = raw_sf.instr_bags[instrument_index].generator_index;
                uint16_t instrument_gen_end = raw_sf.instr_bags[instrument_index + 1].generator_index;

                // Loop over all instrument zone's values
                for (uint16_t instrument_gen_index = instrument_gen_start; instrument_gen_index < instrument_gen_end; instrument_gen_index++) {
                    // Get name and value
                    std::string oper_name = SFGenerator_names[raw_sf.instr_gens[instrument_gen_index].oper];
                    GenAmountType oper_value = raw_sf.instr_gens[instrument_gen_index].amount;
                    instrument_zone_generator_values[oper_name] = oper_value;
                }

                // Does the instrument ID exist?
                if (instrument_zone_generator_values.find("sampleID") == instrument_zone_generator_values.end()) {
                    // If not, this is the global preset zone, save it and go to next preset zone
                    instrument_global_generator_values = instrument_zone_generator_values;
                    continue;
                }

                // Create final zone from default zones
                std::map<std::string, GenAmountType> final_zone_generator_values;
                init_default_zone(final_zone_generator_values);

                // Apply global instrument zone to final zone
                for (auto& entry : instrument_global_generator_values) {
                    final_zone_generator_values[entry.first] = entry.second;
                }

                // Apply current instrument zone to final zone
                for (auto& entry : instrument_zone_generator_values) {
                    final_zone_generator_values[entry.first] = entry.second;
                }

                // Apply global preset zone to current preset zone
                for (auto& entry : preset_global_generator_values) {
                    if (preset_zone_generator_values.find(entry.first) == preset_zone_generator_values.end()) {
                        preset_zone_generator_values[entry.first] = entry.second;
                    }
                }

                // Apply current preset zone
                for (auto& entry : preset_zone_generator_values) {
                    // Get index in flag array from the string
                    int index = -1;
                    while (SFGenerator_names[++index] != entry.first) {}
                    assert(index >= 0 && index < 59);

                    // Get flags
                    GenFlags flags = gen_flags[index];
                    if (flags.instr_only) continue;
                    switch (flags.apply_mode)
                    {
                    case add:
                        final_zone_generator_values[entry.first].s_amount += entry.second.s_amount;
                        break;
                    case clamp_range:
                        final_zone_generator_values[entry.first].ranges.low = std::max(entry.second.ranges.low, final_zone_generator_values[entry.first].ranges.low);
                        final_zone_generator_values[entry.first].ranges.high = std::min(entry.second.ranges.high, final_zone_generator_values[entry.first].ranges.high);
                        break;
                    }
                }

                if (final_zone_generator_values["overridingRootKey"].s_amount == -1)
                {
                    final_zone_generator_values["overridingRootKey"].s_amount = raw_sf.sample_headers[final_zone_generator_values["sampleID"].u_amount].original_key;
                }

                // Parse zone to custom zone format
                Zone new_zone_to_add{
                    final_zone_generator_values["keyRange"].ranges.low,
                    final_zone_generator_values["keyRange"].ranges.high,
                    final_zone_generator_values["velRange"].ranges.low,
                    final_zone_generator_values["velRange"].ranges.high,
                    final_zone_generator_values["sampleID"].u_amount,
                    final_zone_generator_values["startAddrsOffset"].s_amount + final_zone_generator_values["startAddrsCoarseOffset"].s_amount * 32768,
                    final_zone_generator_values["endAddrsOffset"].s_amount + final_zone_generator_values["startAddrsCoarseOffset"].s_amount * 32768,
                    final_zone_generator_values["startloopAddrsOffset"].s_amount + final_zone_generator_values["startAddrsCoarseOffset"].s_amount * 32768,
                    final_zone_generator_values["endloopAddrsOffset"].s_amount + final_zone_generator_values["startAddrsCoarseOffset"].s_amount * 32768,
                    raw_sf.sample_headers[final_zone_generator_values["sampleID"].u_amount].original_key - final_zone_generator_values["overridingRootKey"].s_amount,
                    final_zone_generator_values["sampleModes"].u_amount % 2 == 1,
                    (float)final_zone_generator_values["pan"].s_amount / 500.0f,
                    EnvParams {
                        1.0f / powf(2.0f, (float)final_zone_generator_values["delayVolEnv"].s_amount / 1200.0f),
                        1.0f / powf(2.0f, (float)final_zone_generator_values["attackVolEnv"].s_amount / 1200.0f),
                        1.0f / powf(2.0f, (float)final_zone_generator_values["holdVolEnv"].s_amount / 1200.0f),
                        100.0f / powf(2.0f, (float)final_zone_generator_values["decayVolEnv"].s_amount / 1200.0f),
                        0.0f - final_zone_generator_values["sustainVolEnv"].u_amount / 10.0f,
                        100.0f / powf(2.0f, (float)final_zone_generator_values["releaseVolEnv"].s_amount / 1200.0f),
                    },
                    EnvParams {
                        1.0f / powf(2.0f, (float)final_zone_generator_values["delayModEnv"].s_amount / 1200.0f),
                        1.0f / powf(2.0f, (float)final_zone_generator_values["attackModEnv"].s_amount / 1200.0f),
                        1.0f / powf(2.0f, (float)final_zone_generator_values["holdModEnv"].s_amount / 1200.0f),
                        100.0f / powf(2.0f, (float)final_zone_generator_values["decayModEnv"].s_amount / 1200.0f),
                        0.0f - final_zone_generator_values["sustainModEnv"].u_amount / 10.0f,
                        100.0f / powf(2.0f, (float)final_zone_generator_values["releaseModEnv"].s_amount / 1200.0f),
                    },
                    LfoParams{
                        //freq, intensity, delay
                        8.176f * powf(2.0f, (float)final_zone_generator_values["freqVibLFO"].s_amount / 1200.0f),
                        powf(2.0f, (float)final_zone_generator_values["delayVibLFO"].s_amount / 1200.0f),
                    },
                    LfoParams{
                        //freq, intensity, delay
                        8.176f * powf(2.0f, (float)final_zone_generator_values["freqModLFO"].s_amount / 1200.0f),
                        powf(2.0f, (float)final_zone_generator_values["delayModLFO"].s_amount / 1200.0f),
                    },
                    (float)final_zone_generator_values["modEnvToPitch"].s_amount,
                    (float)final_zone_generator_values["modEnvToFilterFc"].s_amount,
                    (float)final_zone_generator_values["modLfoToPitch"].s_amount,
                    (float)final_zone_generator_values["modLfoToFilterFc"].s_amount,
                    (float)final_zone_generator_values["vibLfoToPitch"].s_amount,
                    (float)final_zone_generator_values["scaleTuning"].s_amount / 100.0f,
                    (float)final_zone_generator_values["coarseTune"].s_amount + (float)final_zone_generator_values["fineTune"].s_amount / 100.0f,
                    (float)final_zone_generator_values["initialAttenuation"].s_amount / 10.0f,
                };

                // Add to final preset
                final_preset.zones.push_back(new_zone_to_add);
            }
        }

        // Add it to the presets
        presets[raw_sf.preset_headers[index].bank << 8 | raw_sf.preset_headers[index].program] = final_preset;

        // Return preset
        return final_preset;
    }

    void Soundfont::init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values) {
        // Zero initialize everything
        for (std::string& str : SFGenerator_names)
            preset_zone_generator_values[str].u_amount = 0x0000;

        // Handle non zero values
        preset_zone_generator_values["initialFilterFc"].s_amount = -12000;
        preset_zone_generator_values["delayModLFO"].s_amount = -12000;
        preset_zone_generator_values["delayVibLFO"].s_amount = -12000;
        preset_zone_generator_values["delayModEnv"].s_amount = -12000;
        preset_zone_generator_values["attackModEnv"].s_amount = -12000;
        preset_zone_generator_values["holdModEnv"].s_amount = -12000;
        preset_zone_generator_values["decayModEnv"].s_amount = -12000;
        preset_zone_generator_values["releaseModEnv"].s_amount = -12000;
        preset_zone_generator_values["delayVolEnv"].s_amount = -12000;
        preset_zone_generator_values["attackVolEnv"].s_amount = -12000;
        preset_zone_generator_values["holdVolEnv"].s_amount = -12000;
        preset_zone_generator_values["decayVolEnv"].s_amount = -12000;
        preset_zone_generator_values["releaseVolEnv"].s_amount = -12000;
        preset_zone_generator_values["keyRange"].ranges = { 0, 127 };
        preset_zone_generator_values["velRange"].ranges = { 0, 127 };
        preset_zone_generator_values["keynum"].s_amount = -1;
        preset_zone_generator_values["velocity"].s_amount = -1;
        preset_zone_generator_values["scaleTuning"].u_amount = 100;
        preset_zone_generator_values["overridingRootKey"].s_amount = -1;
        preset_zone_generator_values["initialAttenuation"].u_amount = 0;
    }

    void Soundfont::clear() {
        // Delete sample data
        free(sample_data);
        samples.clear();
        presets.clear();
    }
    ;

    /*
    int main() {
        Soundfont soundfont;
        soundfont.from_file("../NewSoundFont.sf2");
        return 0;
    }
    */
}