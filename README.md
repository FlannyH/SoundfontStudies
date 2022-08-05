
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
	Flan::Soundfont soundfont;
	soundfont.from_file("path/to/soundfont.sf2");
}
```
## Known issues
- Stereo samples and linked samples can crash the build.
- Instruments and Presets that use any of the sample offset generators can have issues involving wrong loop points and possibly other issues.
## Future plans
- Single header file?

## Data structures
The Soundfont struct exposes two maps:
```c++
std::map<u16, Preset> presets;
std::map<int, Sample> samples;
```

#### Sample
A `Sample` is a data structure that contains:

- A pointer to raw 16-bit signed PCM sample data
- A base sample rate (original root key and fine tuning are already applied here)
- The length of the sample
- The loop start and loop end of the sample
- The number of channels*
#### Preset
A `Preset` is a data structure that only contains a list of `Zone`, a collection of settings meant for a sampler to use.<br>
The map is indexed by a u16, with the bank number in the high byte, and the preset number in the low byte.

#### Zone
A `Zone` is a collection of settings meant for a software sampler. It has:

- Key range
- Velocity range
- Index of the sample (used to index the `Sample` array in `Soundfont`)
- Root key offset
- Sample loop enable flag
- Stereo panning
- Volume envelopes: Delay, Attack, Hold, Decay, Sustain, and Release values. All in either `1.0 / time in seconds` or `Linear volume multiplier`
- Tuning scale: how many semitones there are between each MIDI key
- Initial attenuation** in linear space

To determine which zones to use when playing a note, there are key ranges and velocity ranges. For a given `Preset`, you can loop over each `Zone`, check if the midi key and velocity are in-between or equal to those range values, and if they are, that zone should be used for that note.

*Subject to change
**Not entirely sure how this should be implemented in a synthesizer myself.
