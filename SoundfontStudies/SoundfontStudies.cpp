#include <iostream>
#include "structs.h"
#include <vector>

int main() {
	// Open file
	FILE* in_file;
	fopen_s(&in_file, "../NewSoundFont.sf2", "rb"); // Todo: un-hardcode this
	if (!in_file) { printf("[ERROR] Could not open file!\n"); return 1; }

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
		if (sfbk != "sfbk") { printf("[ERROR] Expected an 'sfbk' chunk, but did not find one!\n"); return 1; }
	}

	printf("---INFO LIST---\n\n");
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
		if (info != "INFO") { printf("[ERROR] Expected an 'INFO' chunk, but did not find one!\n"); return 1; }


		// Handle all chunks in LIST chunk
		while (1) {
			Chunk chunk;
			bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
			if (!should_continue) { break; }

			if (chunk.id == "ifil") { // Soundfont version
				sfVersionTag version;
				curr_chunk_data.get_data(&version, sizeof(version));
				printf("[INFO] Soundfont version: %i.%i\n", version.wMajor, version.wMinor);
			}
			else if (chunk.id == "INAM") { // Soundfont name
				char* soundfont_name = (char*)malloc(chunk.size);
				curr_chunk_data.get_data(soundfont_name, chunk.size);
				printf("[INFO] Soundfont name: '%s'\n", soundfont_name);
			}
			else if (chunk.id == "isng") { // Wavetable sound engine name
				char* wavetable_name = (char*)malloc(chunk.size);
				curr_chunk_data.get_data(wavetable_name, chunk.size);
				printf("[INFO] Wavetable engine name: '%s'\n", wavetable_name);
			}
			else { // Not a chunk we're interested in, skip it
				curr_chunk_data.get_data(nullptr, chunk.size);
			}
		}
	}

	printf("\n---sdta LIST---\n\n");
	// There are 3 LIST chunks. The second one is the sdta list - contains raw sample data
	u8* smpl_data = nullptr;
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
		if (info != "sdta") { printf("[ERROR] Expected an 'sdta' chunk, but did not find one!\n"); return 1; }

		// Handle all chunks in LIST chunk
		while (1) {
			Chunk chunk;
			bool should_continue = chunk.from_chunk_data_handler(curr_chunk_data);
			if (!should_continue) { break; }

			if (chunk.id == "smpl") { // Raw sample data
				smpl_data = (u8*)malloc(chunk.size);
				curr_chunk_data.get_data(smpl_data, chunk.size);
				printf("[INFO] Found sample data, %i bytes total\n", chunk.size);
			}
			else { // Not a chunk we're interested in, skip it (unlikely in this list though)
				curr_chunk_data.get_data(nullptr, chunk.size);
			}
		}
	}

	printf("\n---pdta LIST---\n\n");
	// There are 3 LIST chunks. The second one is the pdta list - this has presets, instruments, and sample header data
	sfPresetHeader*		preset_headers	= nullptr;			int n_preset_headers = 0;
	sfBag*				preset_bags		= nullptr;			int n_preset_bags	 = 0;
	sfModList*			preset_mods		= nullptr;			int n_preset_mods	 = 0;
	sfGenList*			preset_gens		= nullptr;			int n_preset_gens	 = 0;
	sfInst*				instruments		= nullptr;			int n_instruments	 = 0;
	sfBag*				instr_bags		= nullptr;			int n_instr_bags	 = 0;
	sfModList*			instr_mods		= nullptr;			int n_instr_mods	 = 0;
	sfGenList*			instr_gens		= nullptr;			int n_instr_gens	 = 0;
	sfSample*			samples			= nullptr;			int n_samples		 = 0;
	{
		// Read the chunk header
		Chunk curr_chunk;
		curr_chunk.from_file(in_file);
		curr_chunk.verify("LIST");

		// INFO chunk header
		ChunkID info;
		fread_s(&info, sizeof(ChunkID), sizeof(ChunkID), 1, in_file);
		if (info != "pdta") { printf("[ERROR] Expected an 'ptda' chunk, but did not find one!\n"); return 1; }

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
				printf("[INFO] Found %zu preset header", chunk.size / sizeof(sfPresetHeader));
				if (chunk.size / sizeof(sfPresetHeader) > 1) { printf("s"); }
			}
			else if (chunk.id == "pbag") { // Preset bags
				// Allocate enough space and copy the data into it
				preset_bags = (sfBag*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_bags, chunk.size);
				n_preset_bags = chunk.size / sizeof(sfBag);
				printf("[INFO] Found %zu preset bag", chunk.size / sizeof(sfBag));
				if (chunk.size / sizeof(sfBag) > 1) { printf("s"); }
			}
			else if (chunk.id == "pmod") { // Preset modulators
				// Allocate enough space and copy the data into it
				preset_mods = (sfModList*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_mods, chunk.size);
				n_preset_mods = chunk.size / sizeof(sfModList);
				printf("[INFO] Found %zu preset modulator", chunk.size / sizeof(sfModList));
				if (chunk.size / sizeof(sfModList) > 1) { printf("s"); }
			}
			else if (chunk.id == "pgen") { // Preset generator
				// Allocate enough space and copy the data into it
				preset_gens = (sfGenList*)malloc(chunk.size);
				curr_chunk_data.get_data(preset_gens, chunk.size);
				n_preset_gens = chunk.size / sizeof(sfGenList);
				printf("[INFO] Found %zu preset generator", chunk.size / sizeof(sfGenList));
				if (chunk.size / sizeof(sfGenList) > 1) { printf("s"); }
			}
			else if (chunk.id == "inst") { // Preset generator
				// Allocate enough space and copy the data into it
				instruments = (sfInst*)malloc(chunk.size);
				curr_chunk_data.get_data(instruments, chunk.size);
				n_instruments = chunk.size / sizeof(sfInst);
				printf("[INFO] Found %zu instrument", chunk.size / sizeof(sfInst));
				if (chunk.size / sizeof(sfInst) > 1) { printf("s"); }
			}
			else if (chunk.id == "ibag") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_bags = (sfBag*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_bags, chunk.size);
				n_instr_bags = chunk.size / sizeof(sfBag);
				printf("[INFO] Found %zu instrument bag", chunk.size / sizeof(sfBag));
				if (chunk.size / sizeof(sfBag) > 1) { printf("s"); }
			}
			else if (chunk.id == "imod") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_mods = (sfModList*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_mods, chunk.size);
				n_instr_mods = chunk.size / sizeof(sfModList);
				printf("[INFO] Found %zu instrument modulator", chunk.size / sizeof(sfModList));
				if (chunk.size / sizeof(sfModList) > 1) { printf("s"); }
			}
			else if (chunk.id == "igen") { // Preset generator
				// Allocate enough space and copy the data into it
				instr_gens = (sfGenList*)malloc(chunk.size);
				curr_chunk_data.get_data(instr_gens, chunk.size);
				n_instr_gens = chunk.size / sizeof(sfGenList);
				printf("[INFO] Found %zu instrument generator", chunk.size / sizeof(sfGenList));
				if (chunk.size / sizeof(sfGenList) > 1) { printf("s"); }
			}
			else if (chunk.id == "shdr") { // Preset generator
				// Allocate enough space and copy the data into it
				samples = (sfSample*)malloc(chunk.size);
				curr_chunk_data.get_data(samples, chunk.size);
				n_samples = chunk.size / sizeof(sfSample);
				printf("[INFO] Found %zu sample", chunk.size / sizeof(sfSample));
				if (chunk.size / sizeof(sfSample) > 1) { printf("s"); }
			}
			else { // Not a chunk we're interested in, skip it (unlikely in this list though)
				curr_chunk_data.get_data(nullptr, chunk.size);
			}
			printf("\n");
		}
		printf("");
	}

	printf("\n--PRESETS--\n\n");
	for (int p_id = 0; p_id < n_preset_headers - 1; p_id++)
	{
		printf("Preset %02i:\n", p_id);
		printf("\tName: %s\n", preset_headers[p_id].achPresetName);
		printf("\tMIDI program: %03u:%03u\n", preset_headers[p_id].wBank, preset_headers[p_id].wPreset);

		// Get bags
		int n_p_bags = preset_headers[p_id + 1].wPresetBagNdx - preset_headers[p_id].wPresetBagNdx;
		int instr_id = 0;
		for (int p_bag_id = preset_headers[p_id].wPresetBagNdx; p_bag_id < preset_headers[p_id].wPresetBagNdx + n_p_bags; p_bag_id++)
		{
			// Get preset generators
			printf("\tPreset generators:\n");
			int p_gen_id = preset_bags[p_bag_id].wGenNdx;
			int instr_id = 0;
			while (1) {
				printf("\t\t%s = 0x%04X", SFGenerator_names[preset_gens[p_gen_id].sfGenOper].c_str(), preset_gens[p_gen_id].genAmount.u_amount);
				if (preset_gens[p_gen_id].sfGenOper == instrument) {
					instr_id = preset_gens[p_gen_id].genAmount.u_amount;
					printf(" - %s\n", instruments[instr_id].achInstName);
					break;
				}
				p_gen_id++;
				printf("\n");
			}
		}

		int n_i_bags = instruments[instr_id + 1].wInstBagNdx - instruments[instr_id].wInstBagNdx;
		int i_bag_id = instruments[instr_id].wInstBagNdx;
		for (int i_bag_id = instruments[instr_id].wInstBagNdx; i_bag_id < instruments[instr_id].wInstBagNdx + n_i_bags; i_bag_id++)
		{
			// Get preset generators
			printf("\tInstrument generators (bag id %d):\n", i_bag_id);
			
			int n_gen_ids = instr_bags[i_bag_id + 1].wGenNdx - instr_bags[i_bag_id].wGenNdx;
			for (int i_gen_id = instr_bags[i_bag_id].wGenNdx; i_gen_id < instr_bags[i_bag_id].wGenNdx + n_gen_ids; i_gen_id++)
			{	printf("\t\t%s = 0x%04X\n", SFGenerator_names[instr_gens[i_gen_id].sfGenOper].c_str(), instr_gens[i_gen_id].genAmount.u_amount);
			}
		}
	}

	printf("\n--SAMPLES--\n\n");
	int x = 0;
	while (1) {
		printf("\t%s:\n", samples[x].achSampleName);
		printf("\tSample rate: %i\n", samples[x].dwSampleRate);
		printf("\tSample data: %i - %i\n", samples[x].dwStart, samples[x].dwEnd);
		printf("\tSample loop: %i - %i\n", samples[x].dwStartloop, samples[x].dwEndloop);
		if (strcmp(samples[++x].achSampleName, "EOS") == 0)
		{
			break;
		}
	}
}