#include "bobject.hpp"
#include "state.hpp"

namespace liero {

bool BObject::update(State& state) {
	this->pos += this->vel;

	auto ipos = this->pos.cast<i32>();

	if (!state.level.is_inside(ipos)) {
		return false;
	} else {

		Mod& mod = state.mod;

		auto m = state.level.unsafe_mat(ipos);

		if (m.background()) {
			this->vel.y += LF(BObjGravity);
		}

		if (m.dirt_rock()) {
			if (!state.level.graphics.is_empty()) {
				state.level.graphics.unsafe_pixel32(ipos.x, ipos.y) = this->color;
			}

			return false;
		}

		return true;
	}
}

}