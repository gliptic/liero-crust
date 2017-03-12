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

	if (transient_state.might_collide_with_worm(ipos, 5)) {
		auto wr = state.worms.all();

		for (Worm* w; (w = wr.next()) != 0; ) {
			auto wpos = w->pos.cast<i32>();

			if (wpos.x - 5 < ipos.x
			 && wpos.x + 5 > ipos.x
			 && wpos.y - 5 < ipos.y
			 && wpos.y + 5 > ipos.y) {

				if (self.frame == 1) {
					if (w->health < 100) { // TODO: Setting for max health
						w->health = tl::min(w->health + 10, 100); // TODO: Healing randomization
						return false;
					}
				} else if (self.frame == 0) {
					// TODO: if (state.mod.tcdata->bonus_explode_risk())
					return false;
				}
			}
		}
	}

	if (--self.timer <= 0) {
		create(state.mod.get_sobject_type(state.mod.tcdata->bonus_sobj()[self.frame]), state, transient_state, ipos);
		return false;
	}

	return true;
}

}