#include "soundfont.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

#include "envs_lfos.h"

#define VERBOSE 0
#define PRINT_AT_ALL 0

namespace Flan {
    template<class... Args>
    void print([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#if PRINT_AT_ALL
        printf(fmt, args...);
#endif
    }
    template<class... Args>
    void print_verbose([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#if VERBOSE
        print(fmt, args...);
#endif
    }

    float freq32_to_hz(const i32 scale)
    {
        return powf(2.f, (((static_cast<float>(scale) / 65536.f) - 6900.f) / 1200.f) * 440.f);
    }

    float tc32_to_seconds(const i32 scale)
    {
        return powf(2.f, static_cast<float>(scale) / (1200.f * 65536.f));
    }
    float tc32_to_cents(const i32 scale)
    {
        return powf(2.f, static_cast<float>(scale) / (1200.f * 65536.f));
    }

    float fixed32_to_float(const i32 scale) {
        return static_cast<float>(scale) / static_cast<float>(0x10000);
    }

    static void init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values) {
        // Zero initialize everything
        for (std::string& str : SFGenerator_names)
            preset_zone_generator_values[str].u_amount = 0x0000;

        // Handle non zero values
        preset_zone_generator_values["initialFilterFc"].s_amount = 13500;
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

    bool Soundfont::from_file(const std::string& path) {
        const std::string extension = path.substr(path.find_last_of('.'));
        if (extension == ".sf2")
            return from_sf2(path);
        if (extension == ".dls")
            return from_dls(path);
        return false;
    }

    bool Soundfont::from_sf2(const std::string& path)
    {
        // We use this for easy data sharing between functions, without exposing this to the end user
        RawSoundfontData raw_sf{};

        // Open file
        FILE* in_file;
        auto err = fopen_s(&in_file, path.c_str(), "rb");
        if (err) { print("[ERROR] Could not open file! Error code 0x%X\n", err); return false; }
        if (!in_file) { print("[ERROR] Could not open file!\n"); return false; }

        // Read RIFF chunk header
        {
            Chunk riff_chunk;
            riff_chunk.from_file(in_file);
            if (!riff_chunk.verify("RIFF")) return false;
        }

        // There should be a 'sfbk' chunk now
        {
            ChunkId sfbk;
            fread_s(&sfbk, sizeof(ChunkId), sizeof(ChunkId), 1, in_file);
            if (sfbk != "sfbk") { print("[ERROR] Expected an 'sfbk' chunk, but did not find one!\n"); return false; }
        }

        print_verbose("---INFO LIST---\n\n");
        // There are 3 LIST chunks. The first one is the INFO list - this has metadata about the soundfont
        {
            // Read the chunk header
            Chunk curr_chunk;
            curr_chunk.from_file(in_file);
            if (!curr_chunk.verify("LIST")) return false;

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // INFO chunk header
            ChunkId info;
            curr_chunk_data.get_data(&info, sizeof(ChunkId));
            if (info != "INFO") { print("[ERROR] Expected an 'INFO' chunk, but did not find one!\n"); return false; }


            // Handle all chunks in LIST chunk
            while (true) {
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
            if (!curr_chunk.verify("LIST")) return false;

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // INFO chunk header
            ChunkId info;
            curr_chunk_data.get_data(&info, sizeof(ChunkId));
            if (info != "sdta") { print("[ERROR] Expected an 'sdta' chunk, but did not find one!\n"); return false; }

            // Handle all chunks in LIST chunk
            while (true) {
                Chunk chunk;
                bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
                if (!should_continue) { break; }

                if (chunk.id == "smpl") { // Raw sample data
                    _sample_data = static_cast<int16_t*>(malloc(chunk.size));
                    curr_chunk_data.get_data(_sample_data, chunk.size);
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
            if (!curr_chunk.verify("LIST")) return false;

            // INFO chunk header
            ChunkId info;
            fread_s(&info, sizeof(ChunkId), sizeof(ChunkId), 1, in_file);
            if (info != "pdta") { print("[ERROR] Expected an 'ptda' chunk, but did not find one!\n"); return false; }

            // Create a chunk data handler
            ChunkDataHandler curr_chunk_data;
            curr_chunk_data.from_file(in_file, curr_chunk.size);

            // Handle all chunks in LIST chunk
            while (true) {
                Chunk chunk;
                bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
                if (!should_continue) { break; }

                if (chunk.id == "phdr") { // Preset header
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_headers = static_cast<SfPresetHeader*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.preset_headers, chunk.size);
                    raw_sf.n_preset_headers = chunk.size / sizeof(SfPresetHeader);
                    print_verbose("[INFO] Found %zu preset header", chunk.size / sizeof(SfPresetHeader));
                    if (chunk.size / sizeof(SfPresetHeader) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pbag") { // Preset bags
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_bags = static_cast<SfBag*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.preset_bags, chunk.size);
                    raw_sf.n_preset_bags = chunk.size / sizeof(SfBag);
                    print_verbose("[INFO] Found %zu preset bag", chunk.size / sizeof(SfBag));
                    if (chunk.size / sizeof(SfBag) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pmod") { // Preset modulators
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_mods = static_cast<sfModList*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.preset_mods, chunk.size);
                    raw_sf.n_preset_mods = chunk.size / sizeof(sfModList);
                    print_verbose("[INFO] Found %zu preset modulator", chunk.size / sizeof(sfModList));
                    if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "pgen") { // Preset generator
                    // Allocate enough space and copy the data into it
                    raw_sf.preset_gens = static_cast<sfGenList*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.preset_gens, chunk.size);
                    raw_sf.n_preset_gens = chunk.size / sizeof(sfGenList);
                    print_verbose("[INFO] Found %zu preset generator", chunk.size / sizeof(sfGenList));
                    if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "inst") { // Instrument header
                    // Allocate enough space and copy the data into it
                    raw_sf.instruments = static_cast<sfInst*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.instruments, chunk.size);
                    raw_sf.n_instruments = chunk.size / sizeof(sfInst);
                    print_verbose("[INFO] Found %zu instrument", chunk.size / sizeof(sfInst));
                    if (chunk.size / sizeof(sfInst) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "ibag") { // Instrument bag
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_bags = static_cast<SfBag*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.instr_bags, chunk.size);
                    raw_sf.n_instr_bags = chunk.size / sizeof(SfBag);
                    print_verbose("[INFO] Found %zu instrument bag", chunk.size / sizeof(SfBag));
                    if (chunk.size / sizeof(SfBag) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "imod") { // Instrument modulator
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_mods = static_cast<sfModList*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.instr_mods, chunk.size);
                    raw_sf.n_instr_mods = chunk.size / sizeof(sfModList);
                    print_verbose("[INFO] Found %zu instrument modulator", chunk.size / sizeof(sfModList));
                    if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "igen") { // Instrument generator
                    // Allocate enough space and copy the data into it
                    raw_sf.instr_gens = static_cast<sfGenList*>(malloc(chunk.size));
                    curr_chunk_data.get_data(raw_sf.instr_gens, chunk.size);
                    raw_sf.n_instr_gens = chunk.size / sizeof(sfGenList);
                    print_verbose("[INFO] Found %zu instrument generator", chunk.size / sizeof(sfGenList));
                    if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
                }
                else if (chunk.id == "shdr") { // Sample headers
                    // Allocate enough space and copy the data into it
                    raw_sf.sample_headers = static_cast<sfSample*>(malloc(chunk.size));
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
        unsigned int index = 0;
        while (true) {
            Sample new_sample{};

            // Assume MIDI key number 60 to be the base key
            int correction = 60 - raw_sf.sample_headers[index].original_key; // Note space
            correction *= 100; // Cent space
            correction += raw_sf.sample_headers[index].correction;
            float corr_mul = static_cast<float>(correction) / 1200.0f;
            corr_mul = powf(2.0f, corr_mul);
            new_sample.base_sample_rate = static_cast<float>(raw_sf.sample_headers[index].sample_rate) * corr_mul;

            // Allocate memory and copy the sample data into it
            auto start = raw_sf.sample_headers[index].start_index;
            new_sample.length = raw_sf.sample_headers[index].end_index - start;
            new_sample.data = (i16*)&_sample_data[raw_sf.sample_headers[index].start_index];

            // Loop data
            new_sample.loop_start = raw_sf.sample_headers[index].loop_start_index - start;
            new_sample.loop_end = raw_sf.sample_headers[index].loop_end_index - start;
            new_sample.type = raw_sf.sample_headers[index].type;
            if (new_sample.type != monoSample)
                new_sample.linked = (i16*)&_sample_data[raw_sf.sample_headers[raw_sf.sample_headers[index].sample_link].start_index];
            else
                new_sample.linked = nullptr;
            print_verbose("\t%s:\n", raw_sf.sample_headers[index].name);
            print_verbose("\tSample rate (before correction): %i\n", raw_sf.sample_headers[index].sample_rate);
            print_verbose("\tSample rate (after correction): %f\n", new_sample.base_sample_rate);
            print_verbose("\tSample data: %i - %i\n", raw_sf.sample_headers[index].start_index, raw_sf.sample_headers[index].end_index);
            print_verbose("\tSample loop: %i - %i\n", raw_sf.sample_headers[index].loop_start_index, raw_sf.sample_headers[index].loop_end_index);
            print_verbose("\tSample type: %i\n", raw_sf.sample_headers[index].type);
            print_verbose("\tSample link: %i\n", raw_sf.sample_headers[index].sample_link);
            print_verbose("\tPitch correction (cents): %i\n", correction);
            print_verbose("\tPitch multiplier (percent): %0.3f%%\n", corr_mul * 100.0f);

            // Add to map
            samples.push_back(new_sample);

            index++;
            if (strcmp(raw_sf.sample_headers[index].name, "EOS") == 0) {
                break;
            }
            if (index >= raw_sf.n_samples) {
                break;
            }
        }

        print_verbose("\n--PRESETS--\n\n");
        for (unsigned int p_id = 0; p_id < raw_sf.n_preset_headers - 1; p_id++) {
            get_sf2_preset_from_index(p_id, raw_sf);
        }

        if (!raw_sf.sample_headers) {
            return false;
        }

        // Close the file
        const int _ = fclose(in_file);
        (void)_;
        print("Soundfont '%s' loaded succesfully!", path.c_str());

        // Free temporary pointers
        void* pointers_to_clear[] = { raw_sf.preset_headers, raw_sf.preset_bags, raw_sf.preset_mods, raw_sf.preset_gens, raw_sf.instruments, raw_sf.instr_bags, raw_sf.instr_mods, raw_sf.instr_gens, raw_sf.sample_headers };
        for (auto pointer : pointers_to_clear)
            free(pointer);

        return true;
    }

    bool Soundfont::from_dls(const std::string& path)
    {
        // Get a riff tree of the DLS file
        RiffTree riff_tree;
        riff_tree.from_file(path);

        // Get samples
        dls_get_samples(riff_tree);
        _sample_data = reinterpret_cast<int16_t*>(riff_tree.data);

        // Get presets
        {
            // Loop over all instruments in the instrument list
            for (RiffNode& ins : riff_tree["lins"].subchunks) {
                // Init preset and global zone
                Preset preset{};
                Zone global_zone{};

                // Get instrument header
                DlsInsh insh{};
                memcpy_s(&insh, sizeof(insh), ins["insh"].data, ins["insh"].size);

                // Get instrument name
                preset.name = std::string(reinterpret_cast<char*>(ins["INFO"]["INAM"].data));

                // Apply global zone if it exists
                if (ins.exists("lart")) {
                    ChunkDataHandler data;
                    data.from_buffer(ins["lart"]["art1"].data, ins["lart"]["art1"].size);
                    handle_art1(data, global_zone);
                }
                if (ins.exists("lar2")) {
                    ChunkDataHandler data;
                    data.from_buffer(ins["lar2"]["art2"].data, ins["lar2"]["art2"].size);
                    handle_art1(data, global_zone);
                }

                // Loop over individual zones in the instrument
                for (RiffNode& rgn : ins["lrgn"].subchunks) {
                    // Get the zone chunks
                    dlsRgnh rgnh{}; memcpy_s(&rgnh, sizeof(rgnh), rgn["rgnh"].data, rgn["rgnh"].size);
                    dlsWsmp wsmp{}; memcpy_s(&wsmp, sizeof(wsmp), rgn["wsmp"].data, rgn["wsmp"].size);
                    dlsWlnk wlnk{}; memcpy_s(&wlnk, sizeof(wlnk), rgn["wlnk"].data, rgn["wlnk"].size);

                    // Create a zone out of it based on the global zone
                    Zone zone = global_zone;
                    zone.key_range_low = static_cast<uint8_t>(rgnh.key_low);
                    zone.key_range_high = static_cast<uint8_t>(rgnh.key_high);
                    zone.vel_range_low = static_cast<uint8_t>(rgnh.vel_low);
                    zone.vel_range_high = static_cast<uint8_t>(rgnh.vel_high);
                    zone.sample_index = wlnk.smpl_idx;
                    zone.loop_enable = wsmp.loop_mode;

                    // If the zone has an articulator, apply it
                    if (rgn.exists("lart")) {
                        ChunkDataHandler data;
                        data.from_buffer(rgn["lart"]["art1"].data, rgn["lart"]["art1"].size);
                        handle_art1(data, zone);
                    }
                    if (rgn.exists("lar2")) {
                        ChunkDataHandler data;
                        data.from_buffer(rgn["lar2"]["art2"].data, rgn["lar2"]["art2"].size);
                        handle_art1(data, zone);
                    }

                    // Apply wsmp chunk
                    dlsWsmp sample_wsmp{};
                    if (riff_tree["wvpl"][wlnk.smpl_idx].exists("wsmp"))
                        memcpy_s(&sample_wsmp, sizeof(dlsWsmp), riff_tree["wvpl"][wlnk.smpl_idx]["wsmp"].data, riff_tree["wvpl"][wlnk.smpl_idx]["wsmp"].size);
                    zone.root_key_offset = static_cast<int>(sample_wsmp.root_key) - static_cast<int>(wsmp.root_key);
                    zone.sample_loop_start_offset = static_cast<int32_t>(wsmp.loop_start - samples[wlnk.smpl_idx].loop_start);
                    zone.sample_loop_end_offset = static_cast<int32_t>((wsmp.loop_start + wsmp.loop_length) - samples[wlnk.smpl_idx].loop_end);

                    // Add zone to preset
                    preset.zones.push_back(zone);
                }

                // Add the preset to the soundfont
                insh.bank_id >>= 8;
                if ((insh.bank_id & 0x800000) > 0) {
                    insh.bank_id += 128;
                    insh.bank_id -= 0x800000;
                }
                presets[static_cast<uint16_t>(insh.bank_id << 8 | insh.instr_id)] = preset;
            }
        }

        return true;
    }

    void Soundfont::dls_get_samples(Flan::RiffTree& riff_tree)
    {
        // Get ptbl chunk
        Flan::ChunkDataHandler data;
        data.from_buffer(riff_tree["ptbl"].data, riff_tree["ptbl"].size);

        // Get ptbl header
        struct {
            u32 structure_size;
            u32 n_cues;
        } ptbl{};
        data.get_data(&ptbl, sizeof(ptbl));

        // Get pool cues - each pool_table_offset points to a wave-list
        std::vector<u32> pool_table_offsets;
        pool_table_offsets.resize(ptbl.n_cues);
        data.get_data(pool_table_offsets.data(), ptbl.n_cues * sizeof(u32));

        // Get wvpl chunk
        data.from_buffer(riff_tree["wvpl"].data, riff_tree["wvpl"].size);

        // Loop over each entry in the pool table
        samples.resize(pool_table_offsets.size());
        for (size_t i = 0; i < pool_table_offsets.size(); i++) {
            // Get wave chunk
            RiffChunk wave;
            memcpy_s(&wave, sizeof(wave), riff_tree["wvpl"].data + pool_table_offsets[i], sizeof(wave));

            // Create a chunk handler for this for easy access - I'm too paranoid to assume everything is in order after reading the DLS spec
            ChunkDataHandler wave_handler;
            wave_handler.from_buffer(riff_tree["wvpl"].data + pool_table_offsets[i] + sizeof(RiffChunk), wave.size - 4);

            // Here's the data we're hoping to collect from the pool cue
            struct {
                u16 format_tag = 0;
                u16 n_channels = 0;
                u32 sample_rate = 0;
                u32 byte_rate = 0;
                u16 block_align = 0;
                u16 bits_per_sample = 0;
            } fmt;
            struct {
                u32 struct_size = 0;
                u16 root_key = 60;
                i16 fine_tune = 0;
                i32 attenuation = 0;
                u32 options = 0; //ignored
                u32 loop_mode = 0;
            } wsmp;
            struct {
                u32 wsloop_size = 0; //should be 16
                u32 loop_type = 0; // always forward loop
                u32 loop_start = 0; // absolute offset in data chunk
                u32 loop_length = 0;
            } loop_hdr;
            i16* sample_data = nullptr;
            u32 sample_byte_length = 0;

            // Loop over each chunk
            Chunk chunk;
            while (chunk.from_chunk_data_handler(wave_handler)) {
                if (chunk.id == "fmt ") {
                    wave_handler.get_data(&fmt, sizeof(fmt));
                    wave_handler.get_data(nullptr, chunk.size - sizeof(fmt)); // no idea why this is necessary but it is
                }
                else if (chunk.id == "wsmp") {
                    wave_handler.get_data(&wsmp, sizeof(wsmp));
                    if (wsmp.loop_mode == 1) {
                        wave_handler.get_data(&loop_hdr, sizeof(loop_hdr));
                    }
                }
                else if (chunk.id == "data") {
                    sample_data = reinterpret_cast<int16_t*>(wave_handler.data_pointer);
                    wave_handler.get_data(nullptr, chunk.size);
                    sample_byte_length = chunk.size;
                }
                else {
                    wave_handler.get_data(nullptr, chunk.size);
                }
            }

            // Assemble a sample from this
            samples[i] = {
                sample_data,
                sample_data,
                static_cast<float>(fmt.sample_rate) * exp2f(((60.f - static_cast<float>(wsmp.root_key)) / 12.f) + (static_cast<float>(wsmp.fine_tune) / 1200.f)),
                sample_byte_length * fmt.sample_rate / fmt.byte_rate,
                loop_hdr.loop_start,
                loop_hdr.loop_start + loop_hdr.loop_length,
                monoSample
            };
        }
    }
    
    void Soundfont::handle_art1(Flan::ChunkDataHandler& dls_file, Zone& zone) const
    {
        // Get number of connection blocks
        u32 cb_size;
        u32 n_connection_blocks;
        dls_file.get_data(&cb_size, sizeof(u32));
        dls_file.get_data(&n_connection_blocks, sizeof(u32));

        // Loop over all the connection blocks
        for (size_t cb_idx = 0; cb_idx < n_connection_blocks; cb_idx++) {
            struct {
                dlsArtSrc source;
                dlsArtSrc control;
                dlsArtDst destination;
                dlsArtTrn transform;
                i32 scale;
            } block{};
            dls_file.get_data(&block, sizeof(block));

            //LFO Section
            if (block.source == CONN_SRC_NONE       && block.control == CONN_SRC_NONE   && block.destination == CONN_DST_LFO_FREQUENCY) { // LFO frequency
                zone.mod_lfo.freq = freq32_to_hz(block.scale);
            }
            else if (block.source == CONN_SRC_NONE  && block.control == CONN_SRC_NONE   && block.destination == CONN_DST_LFO_STARTDELAY) { // LFO start delay
                zone.mod_lfo.delay = tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_LFO   && block.control == CONN_SRC_NONE   && block.destination == CONN_DST_GAIN) { // LFO attenuation scale
                zone.mod_lfo_to_volume = fixed32_to_float(block.scale) / 10.f;
            }
            else if (block.source == CONN_SRC_LFO   && block.control == CONN_SRC_NONE   && block.destination == CONN_DST_PITCH) { // LFO pitch scale
                zone.mod_lfo_to_pitch = fixed32_to_float(block.scale);
            }
            /*else if (block.source == CONN_SRC_LFO && block.control == CONN_SRC_CC1 && block.destination == CONN_DST_ATTENUATION) { // LFO attenuation scale
                zone.mod_lfo_to_volume = fixed32_to_float(block.scale) / 10.f;
            }
            else if (block.source == CONN_SRC_LFO   && block.control == CONN_SRC_CC1    && block.destination == CONN_DST_PITCH) { // LFO pitch scale
                zone.mod_lfo_to_volume = fixed32_to_float(block.scale);
            }*/ // not sure what to do with these
            // Envelope 1 (volume)
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_DELAYTIME) { // Vol delay
                zone.vol_env.delay = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_ATTACKTIME) { // Vol attack
                zone.vol_env.attack = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_HOLDTIME) { // Vol hold
                zone.vol_env.hold = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_DECAYTIME) { // Vol decay
                zone.vol_env.decay = 96.0f / tc32_to_seconds(block.scale); // 96, since the inferred EG1 attenuation is 96 dB
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_SUSTAINLEVEL) { // Vol sustain
                zone.vol_env.sustain = std::max(-100.f, 6.f * log2f(fixed32_to_float(block.scale) / 1000.f));
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_RELEASETIME) { // Vol release
                zone.vol_env.release = 96.0f / tc32_to_seconds(block.scale);
            }
            /*else if (block.source == CONN_SRC_KEYONVELOCITY && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_ATTACKTIME) { // Vol attack
                zone.vol_env.attack = 1.0f / tc32_to_seconds(block.scale);
            }*/ // no equivalent in sf2 i think
            else if (block.source == CONN_SRC_KEYNUMBER && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG1_DECAYTIME) { // Vol decay
                zone.key_to_vol_env_decay = -static_cast<float>(block.scale) / 65536.f / 128.f; // (65536 for fixed16.16, 128 for number of keys); 
            }
            // Envelope 2 (modulator)
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_DELAYTIME) { // Mod delay
                zone.mod_env.delay = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_ATTACKTIME) { // Mod attack
                zone.mod_env.attack = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_HOLDTIME) { // Mod Hold
                zone.mod_env.hold = 1.0f / tc32_to_seconds(block.scale);
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_DECAYTIME) { // Mod decay
                zone.mod_env.decay = 96.0f / tc32_to_seconds(block.scale); // 96, since the inferred EG1 attenuation is 96 dB
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_SUSTAINLEVEL) { // Mod sustain
                zone.mod_env.sustain = std::max(-100.f, 6.f * log2f(fixed32_to_float(block.scale) / 1000.f));
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_RELEASETIME) { // Mod release
                zone.mod_env.release = 96.0f / tc32_to_seconds(block.scale);
            }
            /*else if (block.source == CONN_SRC_KEYONVELOCITY && block.control == CONN_SRC_NONE && block.destination == CONN_DST_EG2_ATTACKTIME) { // Mod attack
                zone.vol_env.attack = 1.0f / tc32_to_seconds(block.scale);
            }*/ // no equivalent in sf2 i think
            else if (block.source == CONN_SRC_EG2 && block.control == CONN_SRC_NONE && block.destination == CONN_DST_PITCH) { // Mod decay
                zone.mod_env_to_pitch = tc32_to_cents(block.scale); // 96, since the inferred EG1 attenuation is 96 dB
            }
            else if (block.source == CONN_SRC_NONE && block.control == CONN_SRC_NONE && block.destination == CONN_DST_PAN) { // Panning
                zone.pan = fixed32_to_float(block.scale) / 1000.f;
                if (block.scale != 0) {
                    printf("yeet");
                }
            }
            else {
                printf("unknown articulator found!\n");
            }
        }

        // Correct decay based on key vol env decay
        zone.vol_env.decay *= powf(2, zone.key_to_vol_env_decay * (60) / (1200));
    }

    Preset Soundfont::get_sf2_preset_from_index(size_t index, RawSoundfontData& raw_sf) {
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
            if (!preset_zone_generator_values.contains("instrument")) {
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
                if (!instrument_zone_generator_values.contains("sampleID")) {
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
                    if (!preset_zone_generator_values.contains(entry.first)) {
                        preset_zone_generator_values[entry.first] = entry.second;
                    }
                }

                // Apply current preset zone
                for (auto& entry : preset_zone_generator_values) {
                    // Get index in flag array from the string
                    int index2 = -1;
                    while (SFGenerator_names[++index2] != entry.first) {}
                    assert(index2 >= 0 && index2 < 59);

                    // Get flags
                    GenFlags flags = gen_flags[index2];
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
                    default:
                        break;
                    }
                }

                if (final_zone_generator_values["overridingRootKey"].s_amount == -1)
                {
                    final_zone_generator_values["overridingRootKey"].s_amount = raw_sf.sample_headers[final_zone_generator_values["sampleID"].u_amount].original_key;
                }

                // Parse zone to custom zone format
                Zone new_zone_to_add {
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
                    static_cast<u8>(final_zone_generator_values["keynum"].u_amount),
                    static_cast<u8>(final_zone_generator_values["velocity"].u_amount),
                    static_cast<float>(final_zone_generator_values["pan"].s_amount) / 500.0f,
                    EnvParams {
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["delayVolEnv"].s_amount) / 1200.0f),
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["attackVolEnv"].s_amount) / 1200.0f),
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["holdVolEnv"].s_amount) / 1200.0f),
                        100.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["decayVolEnv"].s_amount) / 1200.0f),
                        0.0f - static_cast<float>(final_zone_generator_values["sustainVolEnv"].u_amount) / 10.0f,
                        100.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["releaseVolEnv"].s_amount) / 1200.0f),
                    },
                    EnvParams {
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["delayModEnv"].s_amount) / 1200.0f),
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["attackModEnv"].s_amount) / 1200.0f),
                        1.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["holdModEnv"].s_amount) / 1200.0f),
                        100.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["decayModEnv"].s_amount) / 1200.0f),
                        0.0f - static_cast<float>(final_zone_generator_values["sustainModEnv"].u_amount) / 10.0f,
                        100.0f / powf(2.0f, static_cast<float>(final_zone_generator_values["releaseModEnv"].s_amount) / 1200.0f),
                    },
                    LfoParams{
                        //freq, intensity, delay
                        8.176f * powf(2.0f, static_cast<float>(final_zone_generator_values["freqVibLFO"].s_amount) / 1200.0f),
                        powf(2.0f, static_cast<float>(final_zone_generator_values["delayVibLFO"].s_amount) / 1200.0f),
                    },
                    LfoParams{
                        //freq, intensity, delay
                        8.176f * powf(2.0f, static_cast<float>(final_zone_generator_values["freqModLFO"].s_amount) / 1200.0f),
                        powf(2.0f, static_cast<float>(final_zone_generator_values["delayModLFO"].s_amount) / 1200.0f),
                    },
                    LowPassFilter{
                        8.176f * powf(2.0f, static_cast<float>(final_zone_generator_values["initialFilterFc"].s_amount) / 1200.0f),
                        powf(2, static_cast<float>(final_zone_generator_values["initialFilterQ"].s_amount) / 150.0f),
                    },
                    static_cast<float>(final_zone_generator_values["modEnvToPitch"].s_amount),
                    static_cast<float>(final_zone_generator_values["modEnvToFilterFc"].s_amount),
                    static_cast<float>(final_zone_generator_values["modLfoToPitch"].s_amount),
                    static_cast<float>(final_zone_generator_values["modLfoToFilterFc"].s_amount),
                    static_cast<float>(final_zone_generator_values["modLfoToVolume"].s_amount) / 10.0f,
                    static_cast<float>(final_zone_generator_values["vibLfoToPitch"].s_amount),
                    static_cast<float>(final_zone_generator_values["keynumToVolEnvHold"].s_amount),
                    static_cast<float>(final_zone_generator_values["keynumToVolEnvDecay"].s_amount),
                    static_cast<float>(final_zone_generator_values["keynumToModEnvHold"].s_amount),
                    static_cast<float>(final_zone_generator_values["keynumToModEnvDecay"].s_amount),
                    static_cast<float>(final_zone_generator_values["scaleTuning"].s_amount) / 100.0f,
                    static_cast<float>(final_zone_generator_values["coarseTune"].s_amount) + static_cast<float>(final_zone_generator_values["fineTune"].s_amount) / 100.0f,
                    static_cast<float>(final_zone_generator_values["initialAttenuation"].s_amount) / 10.0f,
                };

                // Add to final preset
                final_preset.zones.push_back(new_zone_to_add);
            }
        }

        // Add it to the presets
        presets[(raw_sf.preset_headers[index].bank << 8) | raw_sf.preset_headers[index].program] = final_preset;

        // Return preset
        return final_preset;
    }

    void Soundfont::clear() {
        // Delete sample data
        free(_sample_data);
        _sample_data = nullptr;
        samples.clear();
        presets.clear();
    };
}