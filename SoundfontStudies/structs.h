#pragma once
#include "common.h"

struct ChunkID {
	union { 
		u32 id; 
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
	bool get_data(void* destination, int byte_count) {
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
	u32 size;
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
	u16 wMajor;
	u16 wMinor;
};

struct sfPresetHeader {
	char achPresetName[20]; // Preset name
	u16 wPreset; // MIDI program number
	u16 wBank; // MIDI bank number
	u16 wPresetBagNdx; // PBAG index - if not equal to PHDR index, the .sf2 file is invalid
	u32 dwLibrary;
	u32 dwGenre;
	u32 dwMorphology;
};

struct sfBag {
	u16 wGenNdx;
	u16 wModNdx;
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
	SFModulator sfModSrcOper;
	SFGenerator sfModDestOper;
	i16 modAmount;
	SFModulator sfModAmtSrcOper;
	SFTransform sfModTransOper;
};

struct sfGenList {
	SFGenerator sfGenOper;
	GenAmountType genAmount;
};
struct sfInst {
	u8 achInstName[20];
	u16 wInstBagNdx;
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
	u8 achSampleName[20];
	u32 dwStart;
	u32 dwEnd;
	u32 dwStartloop;
	u32 dwEndloop;
	u32 dwSampleRate;
	u8 byOriginalKey;
	u8 chCorrection;
	u16 wSampleLink;
	SFSampleLink sfSampleType;
};
#pragma pack(pop)