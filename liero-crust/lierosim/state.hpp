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

//static int const NObjectLimit = 600 + 600;

//typedef FixedObjectList<NObject, NObjectLimit> NObjectList;
typedef FixedObjectList<BObject, 700> BObjectList; // TODO: Configurable limit
typedef FixedObjectList<SObject, 700> SObjectList;
typedef FixedObjectList<Worm, 128> WormList;

struct StateInput {
	StateInput() {
		// TODO
		worm_controls.push_back(ControlState());
		worm_controls.push_back(ControlState());
	}

	tl::Vec<ControlState> worm_controls;
};

struct State {
	Level level;
	Mod& mod;

	NObjectList nobjects;
	BObjectList bobjects;
	SObjectList sobjects; // GFX
	WormList worms;

	Cellphase nobject_broadphase;

	tl::XorShift rand;
	tl::XorShift gfx_rand; // GFX

	u32 current_time;

	void update(gfx::CommonWindow& window);

	State(Mod& mod)
		: mod(mod)
		, current_time(0)
		, nobject_broadphase(NObjectLimit)
		, gfx_rand(0xff) {

		auto r = worms.all();
		u32 index = 0;
		for (Worm* w; (w = r.next()) != 0; ) {
			w->index = index++;
		}
	}
};


}

#endif // LIERO_STATE_HPP
