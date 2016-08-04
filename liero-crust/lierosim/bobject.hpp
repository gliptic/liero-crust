#ifndef LIERO_BOBJECT_HPP
#define LIERO_BOBJECT_HPP 1

#include "config.hpp"
#include <tl/image.hpp>
#include <tl/vector.hpp>

namespace liero {

struct State;

struct BObject {
	u32 color;
	Vector2 pos, vel;

	bool update(State& state);
};

}

#endif // LIERO_BOBJECT_HPP
