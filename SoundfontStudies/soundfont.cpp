#include "soundfont.h"
#include <iostream>
#include "structs.h"
#include <vector>

#define VERBOSE 1

template<class... Args>
void print_verbose(const char* fmt, Args... args)
{
#if VERBOSE
	printf(fmt, args...);
#endif
}

bool Soundfont::from_file(std::string path) {
	// Open file
	FILE* in_file;
	fopen_s(&in_file, path.c_str(), "rb"); // Todo: un-hardcode this
	if (!in_file) { printf("[ERROR] Could not open file!\n"); return false; }

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
		if (sfbk != "sfbk") { printf("[ERROR] Expected an 'sfbk' chunk, but did not find one!\n"); return false; }
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
		if (info != "INFO") { printf("[ERROR] Expected an 'INFO' chunk, but did not find one!\n"); return false; }


		// Handle all chunks in LIST chunk
		while (1) {
			Chunk chunk;
			bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
			if (!should_continue) { break; }

			if (chunk.id == "ifil") { // Soundfont version
				sfVersionTag version;
				curr_chunk_data.get_data(&version, sizeof(version));
				print_verbose("[INFO] Soundfont version: %i.%i\n", version.major, version.minor);
			}
			else if (chunk.id == "INAM") { // Soundfont name
				char* soundfont_name = (char*)malloc(chunk.size);
				curr_chunk_data.get_data(soundfont_name, chunk.size);
				print_verbose("[INFO] Soundfont name: '%s'\n", soundfont_name);
			}
			else if (chunk.id == "isng") { // Wavetable sound engine name
				char* wavetable_name = (char*)malloc(chunk.size);
				curr_chunk_data.get_data(wavetable_name, chunk.size);
				print_verbose("[INFO] Wavetable engine name: '%s'\n", wavetable_name);
			}
			else { // Not a chunk we're interested in, skip it
				curr_chunk_data.get_data(nullptr, chunk.size);
			}
		}
	}

	print_verbose("\n---sdta LIST---\n\n");
	// There are 3 LIST chunks. The second one is the sdta list - contains raw sample data
	u16* smpl_data = nullptr;
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
		if (info != "sdta") { printf("[ERROR] Expected an 'sdta' chunk, but did not find one!\n"); return false; }

		// Handle all chunks in LIST chunk
		while (1) {
			Chunk chunk;
			bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
			if (!should_continue) { break; }

			if (chunk.id == "smpl") { // Raw sample data
				smpl_data = (u16*)malloc(chunk.size);
				curr_chunk_data.get_data(smpl_data, chunk.size);
				print_verbose("[INFO] Found sample data, %i bytes total\n", chunk.size);
			}
			else { // Not a chunk we're interested in, skip it (unlikely in this list though)
				curr_chunk_data.get_data(nullptr, chunk.size);
			}
		}
	}

	print_verbose("\n---pdta LIST---\n\n");
	// There are 3 LIST chunks. The second one is the pdta list - this has presets, instruments, and sample header data
	sfPresetHeader*		preset_headers	= nullptr;			int n_preset_headers = 0;
	sfBag*				preset_bags		= nullptr;			int n_preset_bags	 = 0;
	sfModList*			preset_mods		= nullptr;			int n_preset_mods	 = 0;
	sfGenList*			preset_gens		= nullptr;			int n_preset_gens	 = 0;
	sfInst*				instruments		= nullptr;			int n_instruments	 = 0;
	sfBag*				instr_bags		= nullptr;			int n_instr_bags	 = 0;
	sfModList*			instr_mods		= nullptr;			int n_instr_mods	 = 0;
	sfGenList*			instr_gens		= nullptr;			int n_instr_gens	 = 0;
	sfSample*			sample_headers	= nullptr;			int n_samples		 = 0;
	{
		// Read the chunk header
		Chunk curr_chunk;
		curr_chunk.from_file(in_file);
		curr_chunk.verify("LIST");

		// INFO chunk header
		ChunkID info;
		fread_s(&info, sizeof(ChunkID), sizeof(ChunkID), 1, in_file);
		if (info != "pdta") { printf("[ERROR] Expected an 'ptda' chunk, but did not find one!\n"); return false; }

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
				preset_headers = (sfPresetHeader*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_headers, chunk.size);
				n_preset_headers = chunk.size / sizeof(sfPresetHeader);
				print_verbose("[INFO] Found %zu preset header", chunk.size / sizeof(sfPresetHeader));
				if (chunk.size / sizeof(sfPresetHeader) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "pbag") { // Preset bags
				// Allocate enough space and copy the data into it
				preset_bags = (sfBag*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_bags, chunk.size);
				n_preset_bags = chunk.size / sizeof(sfBag);
				print_verbose("[INFO] Found %zu preset bag", chunk.size / sizeof(sfBag));
				if (chunk.size / sizeof(sfBag) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "pmod") { // Preset modulators
				// Allocate enough space and copy the data into it
				preset_mods = (sfModList*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_mods, chunk.size);
				n_preset_mods = chunk.size / sizeof(sfModList);
				print_verbose("[INFO] Found %zu preset modulator", chunk.size / sizeof(sfModList));
				if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "pgen") { // Preset generator
				// Allocate enough space and copy the data into it
				preset_gens = (sfGenList*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_gens, chunk.size);
				n_preset_gens = chunk.size / sizeof(sfGenList);
				print_verbose("[INFO] Found %zu preset generator", chunk.size / sizeof(sfGenList));
				if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "inst") { // Preset generator
				// Allocate enough space and copy the data into it
				instruments = (sfInst*)malloc(chunk.size);
				curr_chunk_data.get_data(instruments, chunk.size);
				n_instruments = chunk.size / sizeof(sfInst);
				print_verbose("[INFO] Found %zu instrument", chunk.size / sizeof(sfInst));
				if (chunk.size / sizeof(sfInst) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "ibag") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_bags = (sfBag*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_bags, chunk.size);
				n_instr_bags = chunk.size / sizeof(sfBag);
				print_verbose("[INFO] Found %zu instrument bag", chunk.size / sizeof(sfBag));
				if (chunk.size / sizeof(sfBag) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "imod") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_mods = (sfModList*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_mods, chunk.size);
				n_instr_mods = chunk.size / sizeof(sfModList);
				print_verbose("[INFO] Found %zu instrument modulator", chunk.size / sizeof(sfModList));
				if (chunk.size / sizeof(sfModList) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "igen") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_gens = (sfGenList*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_gens, chunk.size);
				n_instr_gens = chunk.size / sizeof(sfGenList);
				print_verbose("[INFO] Found %zu instrument generator", chunk.size / sizeof(sfGenList));
				if (chunk.size / sizeof(sfGenList) > 1) { print_verbose("s"); }
			}
			else if (chunk.id == "shdr") { // Preset generator
				// Allocate enough space and copy the data into it
				sample_headers = (sfSample*)malloc(chunk.size);
				curr_chunk_data.get_data(sample_headers, chunk.size);
				n_samples = chunk.size / sizeof(sfSample);
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

	print_verbose("\n--PRESETS--\n\n");
	for (int p_id = 0; p_id < n_preset_headers - 1; p_id++)
	{
		print_verbose("Preset %02i:\n", p_id);
		print_verbose("\tName: %s\n", preset_headers[p_id].preset_name);
		print_verbose("\tMIDI program: %03u:%03u\n", preset_headers[p_id].bank, preset_headers[p_id].program);

		Preset new_preset;
		// Get bags
		int n_p_bags = preset_headers[p_id + 1].pbag_index - preset_headers[p_id].pbag_index;
		int instr_id = 0;
		for (int p_bag_id = preset_headers[p_id].pbag_index; p_bag_id < preset_headers[p_id].pbag_index + n_p_bags; p_bag_id++)
		{
			// Get preset generators
			print_verbose("\tPreset generators:\n");
			int p_gen_id = preset_bags[p_bag_id].generator_index;
			int instr_id = 0;
			while (1) {
				print_verbose("\t\t%s = 0x%04X", SFGenerator_names[preset_gens[p_gen_id].oper].c_str(), preset_gens[p_gen_id].amount.u_amount);
				if (preset_gens[p_gen_id].oper == instrument) {
					instr_id = preset_gens[p_gen_id].amount.u_amount;
					print_verbose(" - %s\n", instruments[instr_id].name);
					break;
				}
				p_gen_id++;
				print_verbose("\n");
			}
		}

		int n_i_bags = instruments[instr_id + 1].bag_index - instruments[instr_id].bag_index;
		int i_bag_id = instruments[instr_id].bag_index;
		for (int i_bag_id = instruments[instr_id].bag_index; i_bag_id < instruments[instr_id].bag_index + n_i_bags; i_bag_id++)
		{
			// Get preset generators
			print_verbose("\tInstrument generators (bag id %d):\n", i_bag_id);
			
			int n_gen_ids = instr_bags[i_bag_id + 1].generator_index - instr_bags[i_bag_id].generator_index;
			for (int i_gen_id = instr_bags[i_bag_id].generator_index; i_gen_id < instr_bags[i_bag_id].generator_index + n_gen_ids; i_gen_id++)
			{	print_verbose("\t\t%s = 0x%04X\n", SFGenerator_names[instr_gens[i_gen_id].oper].c_str(), instr_gens[i_gen_id].amount.u_amount);
			}
		}
	}

	if (!sample_headers) {
		return false;
	}

	print_verbose("\n--SAMPLES--\n\n");
	// Load all the samples into the list
	int x = 0;
	while (1) {

		Sample new_sample;

		// Assume MIDI key number 60 to be the base key
		int correction = 60 - sample_headers[x].original_key; // Note space
		correction *= 100; // Cent space
		correction += sample_headers[x].correction;
		float corr_mul = (float)correction / 1200.0;
		corr_mul = pow(2, corr_mul);
		new_sample.base_sample_rate = sample_headers[x].sample_rate * corr_mul;

		// Allocate memory and copy the sample data into it
		auto start = sample_headers[x].start_index;
		new_sample.length = sample_headers[x].end_index - start;
		new_sample.n_channels = (sample_headers[x].type == leftSample) ? 2 : 1;
		size_t length = new_sample.length * sizeof(u16) * new_sample.n_channels;
		new_sample.data = (u16*)malloc(new_sample.length * sizeof(u16) * new_sample.n_channels);
		if (new_sample.n_channels == 1) {
			memcpy_s(new_sample.data, length, &smpl_data[sample_headers[x].start_index], length);
		}
		if (new_sample.n_channels == 2) {
			auto p_left = &smpl_data[sample_headers[x].start_index];
			auto p_right = &smpl_data[sample_headers[sample_headers[x].sample_link].start_index];
			for (int i = 0; i < new_sample.length; i += 1) {
				new_sample.data[i * 2 + 0] = *(p_left++);
				new_sample.data[i * 2 + 1] = *(p_right++);
			}
		}

		// Loop data
		new_sample.loop_start = sample_headers[x].loop_start_index - start;
		new_sample.loop_end = sample_headers[x].loop_end_index - start;
		print_verbose("\t%s:\n", sample_headers[x].name);
		print_verbose("\tSample rate (before correction): %i\n", sample_headers[x].sample_rate);
		print_verbose("\tSample rate (after correction): %f\n", new_sample.base_sample_rate);
		print_verbose("\tSample data: %i - %i\n", sample_headers[x].start_index, sample_headers[x].end_index);
		print_verbose("\tSample loop: %i - %i\n", sample_headers[x].loop_start_index, sample_headers[x].loop_end_index);
		print_verbose("\tSample type: %i\n", sample_headers[x].type);
		print_verbose("\tSample link: %i\n", sample_headers[x].sample_link);
		print_verbose("\tPitch correction (cents): %i\n", correction);
		print_verbose("\tPitch multiplier (percent): %0.3f%%\n", corr_mul * 100.0);

		// Add to map
		samples[sample_headers[x].name] = new_sample;
		
		x++;
		// Skip right samples, we handle those together with leftSample
		while (sample_headers[x].type == rightSample) {
			x++;
		}
		if (strcmp(sample_headers[x].name, "EOS") == 0)
		{
			break;
		}
	}

	printf("Soundfont '%s' loaded succesfully!", path.c_str());

	return true;
}

int main()
{
	Soundfont soundfont;
	soundfont.from_file("../NewSoundfont.sf2");
	return 0;
}