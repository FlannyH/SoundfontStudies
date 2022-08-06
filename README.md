
# SoundfontStudies
 A library that can load Soundfont (.sf2) files, and format it neatly so it can be used in applications like a software sampler.
## Features
- 16-bit sample loading
- Full preset parsing, including all the preset and instrument zones
## How to use
- Add the `common.h`, `soundfont.h`, `soundfont.cpp`, and `structs.h` files to your project. In what folder the files are exactly is not important, but make sure all those files are in the same folder together.
- Quick example to load a soundfont:
```c++
int main() {
	// You can do this...
	Flan::Soundfont soundfont1("path/to/soundfont.sf2");
	
	// ...or, alternatively this
	Flan::Soundfont soundfont2;
	soundfont2.from_file("path/to/soundfont.sf2");
}
```
## Known issues
- None! Please report if you find any.
## Future plans
- Single header file?

## Data structures
The Soundfont struct exposes two fields:
```c++
std::map<u16, Preset> presets;
std::vector<Sample> samples;
```

#### Sample
A `Sample` is a data structure that contains:

- A pointer to raw 16-bit signed PCM sample data
- A pointer to the linked sample's 16-bit signed PCM sample data
- A base sample rate (original root key and fine tuning are already applied here)
- The length of the sample
- The loop start and loop end of the sample
- The sample type (used to see if it's mono, the left channel, or the right channel)
#### Preset
A `Preset` is a data structure that only contains a list of `Zone`, a collection of settings meant for a sampler to use.<br>
The map is indexed by a u16, with the bank number in the high byte, and the preset number in the low byte.

#### Zone
A `Zone` is a collection of settings meant for a software sampler. It has:

- Key range
- Velocity range
- Index of the sample (used to index the `Sample` array in `Soundfont`)
- Sample start, end, loop start, and loop end offsets (some soundfonts, notably the ones VGMtrans export use this a lot)
- Root key offset, relative to the `Sample`'s root key
- Sample loop enable flag
- Stereo panning
- Volume envelopes: Delay, Attack, Hold, Decay, Sustain, and Release values. All in either `1.0 / time in seconds` or `Linear volume multiplier`
- Tuning scale: how many semitones there are between each MIDI key
- Initial attenuation* in linear space (not sure how to properly implement this in a synthesizer myself)

To determine which zones to use when playing a note, there are key ranges and velocity ranges. For a given `Preset`, you can loop over each `Zone`, check if the midi key and velocity are in-between or equal to those range values, and if they are, that zone should be used for that note.
