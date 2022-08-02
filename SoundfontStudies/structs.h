#pragma once
#include "common.h"

struct ChunkID {
	union { 
		u32 id = 0; 
		char id_chr[4];
	};
	static constexpr u32 from_string(const char* v) { 
		return *(u32*)v;
	}
	bool operator==(const ChunkID& rhs) { return id == rhs.id; }
	bool operator!=(const ChunkID& rhs) { return id == rhs.id; }
	bool operator==(const char* rhs) { return *(u32*)rhs == id; }
	bool operator!=(const char* rhs) { return *(u32*)rhs != id; }
};

struct ChunkDataHandler
{
	u8* data_pointer = nullptr;
	u32 chunk_bytes_left = 0;
	bool from_buffer(u8* buffer_to_use, u32 size) {
		data_pointer = buffer_to_use;
		chunk_bytes_left = size;
	}
	bool from_file(FILE* file, u32 size)
	{
		if (size == 0)
		{
			return false;
		}
		data_pointer = (u8*)malloc(size);
		if (data_pointer == 0) { return false; }
		fread_s(data_pointer, size, size, 1, file);
		chunk_bytes_left = size;
		return true;
	}
	bool get_data(void* destination, u32 byte_count) {
		if (chunk_bytes_left >= byte_count)
		{
			if (destination != nullptr) { // Sending a nullptr is valid, it just discards byte_count number of bytes
				memcpy(destination, data_pointer, byte_count);
			}
			chunk_bytes_left -= byte_count;
			data_pointer += byte_count;
			return true;
		}
		return false;
	}
};

struct Chunk {
	ChunkID id;
	u32 size = 0;
	bool from_file(FILE*& file) {
		return fread_s(this, sizeof(*this), sizeof(*this), 1, file) > 0;
	}
	bool from_chunk_data_handler(ChunkDataHandler& chunk_data_handler)
	{
		return chunk_data_handler.get_data(this, sizeof(*this));
	}
	bool verify(const char* other_id) {
		if (id != other_id) {
			printf("[ERROR] Chunk ID mismatch! Expected '%s'\n", other_id);
		}
		return id == other_id;
	}
};
#pragma pack(push, 1)
struct sfVersionTag {
	u16 major;
	u16 minor;
};

struct sfPresetHeader {
	char preset_name[20]; // Preset name
	u16 program; // MIDI program number
	u16 bank; // MIDI bank number
	u16 pbag_index; // PBAG index - if not equal to PHDR index, the .sf2 file is invalid
	u32 library;
	u32 genre;
	u32 morphology;
};

struct sfBag {
	u16 generator_index;
	u16 modulator_index;
};

struct SFModulator
{
	u16 index : 7;
	u16 cc_flag : 1;
	u16 direction : 1;
	u16 polarity : 1;
	u16 type : 6;
};

struct RangesType {
	u8 low;
	u8 high;
};

union GenAmountType {
	RangesType ranges;
	u16 u_amount;
	i16 s_amount;
};

enum SFGenerator : u16 {
	startAddrsOffset = 0,
	endAddrsOffset,
	startloopAddrsOffset,
	endloopAddrsOffset,
	startAddrsCoarseOffset,
	modLfoToPitch,
	vibLfoToPitch,
	modEnvToPitch,
	initialFilterFc,
	initialFilterQ,
	modLfoToFilterFc,
	modEnvToFilterFc,
	endAddrsCoarseOffset,
	modLfoToVolume,
	unused1,
	chorusEffectsSend,
	reverbEffectsSend,
	pan,
	unused2,
	unused3,
	unused4,
	delayModLFO,
	freqModLFO,
	delayVibLFO,
	freqVibLFO,
	delayModEnv,
	attackModEnv,
	holdModEnv,
	decayModEnv,
	sustainModEnv,
	releaseModEnv,
	keynumToModEnvHold,
	keynumToModEnvDecay,
	delayVolEnv,
	attackVolEnv,
	holdVolEnv,
	decayVolEnv,
	sustainVolEnv,
	releaseVolEnv,
	keynumToVolEnvHold,
	keynumToVolEnvDecay,
	instrument,
	reserved1,
	keyRange,
	velRange,
	startloopAddrsCoarseOffset,
	keynum,
	velocity,
	initialAttenuation,
	reserved2,
	endloopAddrsCoarseOffset,
	coarseTune,
	fineTune,
	sampleID,
	sampleModes,
	reserved3,
	scaleTuning,
	exclusiveClass,
	overridingRootKey,
	unused5,
	endOper
};

std::string SFGenerator_names[] {
	"startAddrsOffset",
	"endAddrsOffset",
	"startloopAddrsOffset",
	"endloopAddrsOffset",
	"startAddrsCoarseOffset",
	"modLfoToPitch",
	"vibLfoToPitch",
	"modEnvToPitch",
	"initialFilterFc",
	"initialFilterQ",
	"modLfoToFilterFc",
	"modEnvToFilterFc",
	"endAddrsCoarseOffset",
	"modLfoToVolume",
	"unused1",
	"chorusEffectsSend",
	"reverbEffectsSend",
	"pan",
	"unused2",
	"unused3",
	"unused4",
	"delayModLFO",
	"freqModLFO",
	"delayVibLFO",
	"freqVibLFO",
	"delayModEnv",
	"attackModEnv",
	"holdModEnv",
	"decayModEnv",
	"sustainModEnv",
	"releaseModEnv",
	"keynumToModEnvHold",
	"keynumToModEnvDecay",
	"delayVolEnv",
	"attackVolEnv",
	"holdVolEnv",
	"decayVolEnv",
	"sustainVolEnv",
	"releaseVolEnv",
	"keynumToVolEnvHold",
	"keynumToVolEnvDecay",
	"instrument",
	"reserved1",
	"keyRange",
	"velRange",
	"startloopAddrsCoarseOffset",
	"keynum",
	"velocity",
	"initialAttenuation",
	"reserved2",
	"endloopAddrsCoarseOffset",
	"coarseTune",
	"fineTune",
	"sampleID",
	"sampleModes",
	"reserved3",
	"scaleTuning",
	"exclusiveClass",
	"overridingRootKey",
	"unused5",
	"endOpe",
};

enum SFTransform : u16 {
	linear = 0,
	absolute_value = 2,
};

struct sfModList {
	SFModulator src_oper;
	SFGenerator dest_oper;
	i16 amount;
	SFModulator amount_src_oper;
	SFTransform trans_oper;
};

struct sfGenList {
	SFGenerator oper;
	GenAmountType amount;
};
struct sfInst {
	u8 name[20];
	u16 bag_index;
};

enum SFSampleLink : u16 {
	monoSample = 1,
	rightSample = 2,
	leftSample = 4,
	linkedSample = 8,
	RomMonoSample = 0x8001,
	RomRightSample = 0x8002,
	RomLeftSample = 0x8004,
	RomLinkedSample = 0x8008
};

struct sfSample {
	char name[20];
	u32 start_index;
	u32 end_index;
	u32 loop_start_index;
	u32 loop_end_index;
	u32 sample_rate;
	u8 original_key;
	u8 correction;
	u16 sample_link;
	SFSampleLink type;
};
#pragma pack(pop)