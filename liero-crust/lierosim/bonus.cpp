#include "bonus.hpp"

#include "state.hpp"

namespace liero {

void update(State& state, Bonus& self) {

	self.pos.y += self.vel_y;

	auto ipos = self.pos.cast<i32>();

	if (state.level.mat_wrap_back(ipos + tl::VectorI2(0, 1))) {
		self.vel_y += state.mod.tcdata->bonus_gravity();
	}

	// TODO
}

}