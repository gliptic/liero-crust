#ifndef LIERO_STATE_HPP
#define LIERO_STATE_HPP 1

#include "mod.hpp"
#include "level.hpp"
#include "object_list.hpp"
#include "nobject.hpp"
#include "bobject.hpp"
#include "sobject.hpp"
#include "worm.hpp"
#include "cellphase.hpp"
#include <tl/rand.hpp>
#include <tl/vec.hpp>
#include "../gfx/window.hpp"

namespace liero {

typedef FixedObjectList<BObject, 700> BObjectList; // TODO: Configurable limit
typedef FixedObjectList<SObject, 700> SObjectList;
typedef FixedObjectList<Worm, 128> WormList;

typedef void (*PlaySoundFunc)(liero::ModRef& mod, i16 sound_index, TransientState& transient_state);

#define PROFILE 1

inline u32 rotl(u32 x, u32 v) {
	return (x << v) | (x >> (32 - v));
}

struct TransientState {
	TransientState(usize worm_count, PlaySoundFunc play_sound_init, void* sound_user_data_init)
		: play_sound(play_sound_init)
		, sound_user_data(sound_user_data_init)
		, graphics(true)
#if PROFILE
		, col_mask_tests(0), col_tests(0), col2_tests(0)
#endif
		{

		for (usize i = 0; i < worm_count; ++i) {
			worm_state.push_back(WormTransientState());
		}
	}

	tl::Vec<WormTransientState> worm_state;

#if PROFILE
	u32 col_mask_tests, col_tests, col2_tests;
#endif

	u32 worm_bloom_x, worm_bloom_y;

	bool might_collide_with_worm(tl::VectorI2 ipos, i32 detect_distance) {
		u32 xsh1 = (ipos.x + 8) >> 4;
		u32 ysh1 = (ipos.y + 8) >> 4;

		u32 max = ((detect_distance + 7 + 15) >> 4);

		u32 mask = (1 << (max & 31)) - 1;
		u32 mask2 = mask | (mask << (32 - max));

		u32 xmask = rotl(mask2, xsh1);
		u32 ymask = rotl(mask2, ysh1);

		++this->col_mask_tests;

		return (this->worm_bloom_x & xmask) && (this->worm_bloom_y & ymask);
	}

	PlaySoundFunc play_sound;
	void* sound_user_data;
	bool graphics;
};

struct State {
	ModRef mod;
	Level level;

	NObjectList nobjects;
	BObjectList bobjects;
	SObjectList sobjects; // GFX
	WormList worms;

	Cellphase nobject_broadphase;

	tl::LcgPair rand;
	tl::LcgPair gfx_rand; // GFX
	
	u32 current_time;

	void update(TransientState& transient_state);

	void copy_from(State const& other, bool copy_graphics = false) {
		this->level.copy_from(other.level, copy_graphics);
		this->nobjects = other.nobjects;
		this->worms = other.worms;
		this->nobject_broadphase.copy_from(other.nobject_broadphase, nobjects.size());
		this->rand = other.rand;
		this->current_time = other.current_time;
		this->gfx_rand = other.gfx_rand; // Might as well copy this always because it's cheap

		if (copy_graphics) {
			this->sobjects = other.sobjects;
			this->bobjects = other.bobjects;
		} else {
			this->sobjects.clear();
			this->bobjects.clear();
		}
	}

	State(ModRef mod)
		: mod(mod)
		, current_time(0)
		, nobject_broadphase(NObjectLimit)
		, rand(0xaaaaaa, 0xbbbbbb)
		, gfx_rand(0xf0f0f0, 0x777777) {
	}
};


}

#endif // LIERO_STATE_HPP
