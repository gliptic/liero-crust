#ifndef LIERO_SFX_MIXER_HPP
#define LIERO_SFX_MIXER_HPP 1

#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/memory.h>

namespace sfx {

typedef tl::VecSlice<i16> Sound;

struct Channel {
	// Mutator write once, mixer read
	Sound const* sound;

	// Mutator write once, mutator read, mixer write
	u32 id;

	// Mutator write, mixer read
	u32 flags;

	// Mutator write once, mixer read/write
	u32 soundpos;
	u32 pos;
	u32 volumes;

	/*
	Mutator can change flags from 0, signaling a new active channel
	Mixer or mutator can change flags to 0, signaling a new inactive channel
	*/
};

struct Mixer {
	static u32 const invalid_id = 0;
	static u32 const inactive_flags = 0;
	static usize const ChannelCount = 8;

	Channel channel_states[ChannelCount];
	i32 base_frame;

	Mixer() {
		memset(this, 0, sizeof(*this));
	}

	u32 now() {
		return this->base_frame;
	}

	bool is_playing(u32 id) {
		i32 ch_idx = find_channel(id);

		return ch_idx >= 0;
	}

	void stop(u32 id) {
		assert(id != invalid_id);

		i32 ch_idx = find_channel(id);

		if (ch_idx >= 0) {
			this->channel_states[ch_idx].flags = inactive_flags;
		}
	}

	u32 play(Sound const* sound, u32 time, u32 id, u32 flags);
	void mix(i16* out, u32 frame_count);
	i32 find_channel(u32 id);
};



}

#endif // LIERO_SFX_MIXER_HPP

