#pragma once
#include <unordered_map>
#include "common.h"
#include "fixed_32_32.h"

struct Sample {
	u16* data;						// Can not be nullptr 
	fixed_32_32 base_sample_rate;	// In samples per second
	u32 length;						// In samples
	u32 loop_start;					// In samples
	u32 loop_end;					// In samples
	u8 n_channels;
};

struct Generator {
	u32 sample_index;
};

struct PresetIndex {
	u8 bank;
	u8 program;
};
struct PresetIndexCompare
{
	bool operator()(const PresetIndex& lhs, const PresetIndex& rhs)
	{
		// If banks are not equal, ignore program and just compare the banks
		if (lhs.bank != rhs.bank) { return lhs.bank < rhs.bank; }

		// If banks are equal, compare programs
		return lhs.program < rhs.program;
	}
};

struct Preset {
	std::vector<Generator> generators;
};

struct Soundfont
{
	std::unordered_map<PresetIndex, Preset, PresetIndexCompare> presets;
	std::vector<Sample> samples;
	bool from_file(std::string path);
};