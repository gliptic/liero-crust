#include "mixer.hpp"

namespace sfx {


i32 Mixer::find_channel(u32 id) {
	i32 c;

	for (c = 0; c < Mixer::ChannelCount; ++c) {
		if (this->channel_states[c].id == id)
			return (i32)c;
	}

	return -1;
}

static i32 find_free_channel(Mixer& self) {
	u32 c;

	for (c = 0; c < Mixer::ChannelCount; ++c) {
		if (self.channel_states[c].id == Mixer::invalid_id)
			return (i32)c;
	}

	return -1;
}

u32 Mixer::play(Sound const* sound, u32 time, u32 id, u32 flags) {

	assert(id != invalid_id && flags != inactive_flags);

	i32 ch_idx = find_free_channel(*this);

	if (ch_idx >= 0) {
		Channel* ch = this->channel_states + ch_idx;
		ch->sound = sound;
		ch->pos = time;
		ch->soundpos = 0;
		ch->volumes = 0x10001000;
		ch->flags = flags;

		TL_WRITE_SYNC(); // Make sure id is the last write

		ch->id = id;
		return id;
	}

	return 0;
}

inline i16 saturate(i32 x) {
	if (x < -32768)
		return -32768;
	else if (x > 32767)
		return 32767;
	return (i16)x;
}

static bool add_channel(i16* out, u32 frame_count, u32 now, Channel* ch) {

	Sound const* sound = ch->sound;
	u32 pos = ch->pos;

	// TODO: Get rid of undefined cast
	i32 diff = i32(pos - now);
	i32 relbegin = diff;
	u32 soundlen = u32(sound->size());
	u32 flags = ch->flags;

	if (relbegin < 0)
		relbegin = 0;
	
	i32 scaler = (ch->volumes & 0x1fff);
	u32 soundpos = ch->soundpos;

	TL_READ_SYNC();

	if (ch->id == Mixer::invalid_id)
		return false;

	// TODO: ch might be overwritten by another sound from this point.
	// 

	for (u32 cur = (u32)relbegin; cur < frame_count; ++cur) {
		i32 soundsamp = (*sound)[soundpos];
		i32 sampa = out[cur * 2];
		i32 sampb = out[cur * 2 + 1];

		out[cur * 2] = saturate(sampa + ((soundsamp * scaler) >> 12));
		out[cur * 2 + 1] = saturate(sampb + ((soundsamp * scaler) >> 12));
		++soundpos;

		if (soundpos >= soundlen) {
#if 0 // TODO
			if (flags & SFX_SOUND_LOOP)
				soundpos = 0;
			else
#endif
				return false;
		}
	}

	if (ch->id == Mixer::invalid_id)
		return false; // Stop

	ch->soundpos = soundpos;

	return true;
}

void Mixer::mix(i16* out, u32 frame_count) {

	memset(out, 0, frame_count * 2 * sizeof(i16));

	// TODO: Handle discontinuous mixing.
	// i.e. if (now + frame_count) in one call differs from (now) in the next,
	// we need to advance soundpos for channels to be accurate.

	for (u32 c = 0; c < ChannelCount; ++c) {
		Channel* ch = this->channel_states + c;

		if (ch->id != invalid_id) {

			if (!add_channel(out, frame_count, this->base_frame, ch)) {
				// Remove
				//ch->flags = inactive_flags;
				ch->id = invalid_id;
				TL_WRITE_SYNC(); // Make sure ch->flags and ch->id writes are visible
			}
		}
	}

	this->base_frame += frame_count;
}

}