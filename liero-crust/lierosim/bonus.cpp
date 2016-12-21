#include "bonus.hpp"

#include "state.hpp"

namespace liero {

bool update(State& state, Bonus& self, TransientState& transient_state) {

	self.pos.y += self.vel_y;

	auto ipos = self.pos.cast<i32>();

	if (state.level.mat_wrap_back(ipos + tl::VectorI2(0, 1))) {
		self.vel_y += state.mod.tcdata->bonus_gravity();
	}
	
	i32 inew_y = i32(self.pos.y + self.vel_y);

	if (state.level.mat_dirt_rock(tl::VectorI2((i32)self.pos.x, inew_y))) {

		self.vel_y *= state.mod.tcdata->bonus_bounce_mult();

		if (abs(self.vel_y.raw()) < 100) {
			self.vel_y = Scalar();
		}
	}

	if (--self.timer <= 0) {
		// TODO: Correct sobject
		create(state.mod.get_sobject_type(0), state, transient_state, ipos);
		return false;
	}

	return true;
}

}