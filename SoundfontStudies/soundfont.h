#pragma once
#include <unordered_map>
#include "common.h"
#include <map>

struct Sample {
	u16* data;						// Can not be nullptr 
	float base_sample_rate;			// In samples per second
	u32 length;						// In samples
	u32 loop_start;					// In samples
	u32 loop_end;					// In samples
	u8 n_channels;
};

struct Zone {
	u32 sample_index;
	u8 vel_range_low;
	u8 vel_range_high;
	u8 key_range_low;
	u8 key_range_high;
	u8 sample_mode;
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
	std::vector<Zone> generators;
};

struct Soundfont
{
	std::unordered_map<PresetIndex, Preset, PresetIndexCompare> presets;
	std::map<std::string, Sample> samples;
	bool from_file(std::string path);
};