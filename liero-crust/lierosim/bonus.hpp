#ifndef LIERO_BONUS_HPP
#define LIERO_BONUS_HPP 1

#include <liero-sim/config.hpp>

namespace liero {

struct State;
struct TransientState;

struct Bonus {
	Vector2 pos;
	Scalar vel_y;

	u32 frame, timer, weapon;
};

bool update(State& state, Bonus& self, TransientState& transient_state);

}

#endif // LIERO_BONUS_HPP
