#ifndef LIERO_NOBJECT_HPP
#define LIERO_NOBJECT_HPP 1

#include <liero-sim/config.hpp>
#include <tl/cstdint.h>

#include "object_list.hpp"
#include "level.hpp"

namespace liero {

struct State;
struct NObjectType;

struct NObject {

	enum ObjState {
		Alive,
		Removed,
		Exploded
	};

	Vector2 pos, vel;
	u32 ty_idx, time_to_die, cur_frame;

	u16 cell;

	static void explode_obj(NObjectType const& ty, Vector2 pos, Vector2 vel, State& state);
};

static int const NObjectLimit = 600 + 600;

typedef FixedObjectList<NObject, NObjectLimit> NObjectList;

void create(NObjectType const& self, State& state, Scalar angle, Vector2 pos, Vector2 vel, tl::Color override_color = tl::Color(0));
bool update(NObject& self, State& state, NObjectList::Range& range);

}

#endif // LIERO_NOBJECT_HPP
