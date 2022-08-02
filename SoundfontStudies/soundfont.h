#pragma once
#include <unordered_map>
#include <map>
#include "common.h"
#include "structs.h"

struct Sample {
	u16* data;						// Pointer to the sample data. Should always be a valid pointer
	float base_sample_rate;			// In samples per second
	u32 length;						// In samples
	u32 loop_start;					// In samples
	u32 loop_end;					// In samples
	u8 n_channels;					// Number of audio channels, 1 = mono, 2 = stereo
};

struct Zone {
	u8 key_range_low = 0;		// Lowest MIDI key in this zone
	u8 key_range_high = 127;	// Highest MIDI key in this zone
	u8 vel_range_low = 0;		// Lowest possible velocity in this zone
	u8 vel_range_high = 0;		// Highest possible velocity in this zone
	u32 sample_index = 0;		// Which sample in the Soundfont::samples array the zone uses
	bool loop_enable = false;	// True if sample loops, False if sample does not loop
	float pan = 0.0f;			// -1.0f for full left, +1.0f for full right, 0.0f for center
	float attack = 0.001;		// Attack duration in seconds
	float hold = 0.001;			// Hold duration in seconds
	float decay = 0.001;		// Decay duration in seconds
	float sustain = 0.0;		// Sustain volume in dB
	float release = 0.001;		// Release duration in seconds
	float scale_tuning = 1.0f;	// Difference in semitones between each MIDI note
	float tuning = 0.0f;		// Combination of the sf2 coarse and fine tuning, could be added to MIDI key directly to get corrected pitch
};

struct PresetIndex {
	u8 bank;
	u8 program;
};

struct Preset {
	std::vector<Zone> zones;
};

struct Soundfont
{
public:
	std::map<u16, Preset> presets;
	std::map<std::string, Sample> samples;
	bool from_file(std::string path);
	Preset get_preset_from_index(size_t index);
	void init_default_zone(std::map<std::string, GenAmountType>& preset_zone_generator_values);
private:
	sfPresetHeader*		preset_headers	= nullptr;			int n_preset_headers = 0;
	sfBag*				preset_bags		= nullptr;			int n_preset_bags	 = 0;
	sfModList*			preset_mods		= nullptr;			int n_preset_mods	 = 0;
	sfGenList*			preset_gens		= nullptr;			int n_preset_gens	 = 0;
	sfInst*				instruments		= nullptr;			int n_instruments	 = 0;
	sfBag*				instr_bags		= nullptr;			int n_instr_bags	 = 0;
	sfModList*			instr_mods		= nullptr;			int n_instr_mods	 = 0;
	sfGenList*			instr_gens		= nullptr;			int n_instr_gens	 = 0;
	sfSample*			sample_headers	= nullptr;			int n_samples		 = 0;
};