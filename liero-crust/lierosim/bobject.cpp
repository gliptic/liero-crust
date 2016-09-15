#include "bobject.hpp"
#include "state.hpp"

namespace liero {

// TODO: We can use bobjects in place of particle__disappearing if
// it were possible to adapt the gravity and disable painting into the level.

void BObject::create(State& state, Vector2 pos, Vector2 vel) {
	auto& bobj = *state.bobjects.new_object_reuse();

	ModRef& mod = state.mod;

	bobj.color = mod.pal.entries[mod.tcdata->first_blood_colour() + state.gfx_rand.get_i32(mod.tcdata->num_blood_colours())].v;
	bobj.pos = pos;
	bobj.vel = vel;
}

bool BObject::update(State& state) {
	this->pos += this->vel;

	auto ipos = this->pos.cast<i32>();

	if (!state.level.is_inside(ipos)) {
		return false;
	} else {

		ModRef& mod = state.mod;

		if (state.level.mat_dirt_rock(ipos)) {
			state.level.graphics.unsafe_pixel32(ipos.x, ipos.y) = this->color;

			return false;
		}

		this->vel.y += mod.tcdata->bobj_gravity();

		return true;
	}
}

}