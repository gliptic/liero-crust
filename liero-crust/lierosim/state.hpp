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

typedef void (*PlaySoundFunc)(liero::ModRef& mod, TransientState& transient_state);

struct TransientState {
	TransientState(usize worm_count, PlaySoundFunc play_sound_init, void* sound_user_data_init)
		: play_sound(play_sound_init)
		, sound_user_data(sound_user_data_init) {

		for (usize i = 0; i < worm_count; ++i) {
			worm_state.push_back(WormTransientState());
		}
	}

	tl::Vec<WormTransientState> worm_state;

	PlaySoundFunc play_sound;
	void* sound_user_data;
};

#define PROFILE 1

struct State {
	Level level;
	ModRef mod;

	NObjectList nobjects;
	BObjectList bobjects;
	SObjectList sobjects; // GFX
	WormList worms;

	Cellphase nobject_broadphase;

	tl::LcgPair rand;
	tl::LcgPair gfx_rand; // GFX
	
	u32 current_time;

#if PROFILE
	u32 col_mask_tests, col_tests, col2_tests;
#endif

	u32 worm_bloom_x, worm_bloom_y; // TEMP: This is transient. Shouldn't be in State

	void update(TransientState& transient_state);

	State(ModRef mod)
		: mod(mod)
		, current_time(0)
		, nobject_broadphase(NObjectLimit)
		, gfx_rand(0xf0f0f0, 0x777777)
#if PROFILE
		, col_mask_tests(0), col_tests(0), col2_tests(0)
#endif
	{
	/*
		auto r = worms.all();
		u32 index = 0;
		for (Worm* w; (w = r.next()) != 0; ) {
			w->index = index++;
	*/
	}
};


}

#endif // LIERO_STATE_HPP
