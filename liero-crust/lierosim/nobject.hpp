#ifndef LIERO_NOBJECT_HPP
#define LIERO_NOBJECT_HPP 1

#include <liero-sim/config.hpp>
#include <tl/cstdint.h>

#include "object_list.hpp"
#include "level.hpp"

namespace liero {

struct State;
struct TransientState;
struct NObjectType;

struct NObject {

	enum ObjState {
		Alive,
		Removed,
		Exploded
	};

	Vector2 pos, vel;
	u32 time_to_die;

	u32 cur_frame; // This is sometimes used to store colors, so we can't reduce it to 16 bits unless we change that
	u16 ty_idx;
	i16 cell;
	i16 owner;

	// Set cell to an invalid value so the update works
	NObject() {
	}

	static void explode_obj(NObjectType const& ty, Vector2 pos, Vector2 vel, i16 owner, State& state, TransientState& transient_state);
};

//static int const NObjectLimit = 600 + 600;
static int const NObjectLimit = 2 * (600 + 600);

typedef FixedObjectList<NObject, NObjectLimit> NObjectList;

TL_NEVER_INLINE void create(NObjectType const& self, State& state, Scalar angle, Vector2 pos, Vector2 vel, i16 owner = -1, tl::Color override_color = tl::Color(0));
TL_NEVER_INLINE bool update(NObject& self, State& state, NObjectList::Range& range, TransientState& transient_state);

}

#endif // LIERO_NOBJECT_HPP
