#ifndef LIERO_BOBJECT_HPP
#define LIERO_BOBJECT_HPP 1

#include <liero-sim/config.hpp>
#include <tl/gfx/image.hpp>
#include <tl/vector.hpp>

namespace liero {

struct State;

struct BObject {
	u32 color;
	Vector2 pos, vel;

	static void create(State& state, Vector2 pos, Vector2 vel);
	bool update(State& state);
};

}

#endif // LIERO_BOBJECT_HPP
