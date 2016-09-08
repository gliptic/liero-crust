#include "state.hpp"

#include "../gfx/buttons.hpp"

namespace liero {

void State::update(TransientState& transient_state) {

	transient_state.worm_bloom_x = 0;
	transient_state.worm_bloom_y = 0;

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			u32 xr = w->pos.x.raw(),
				yr = w->pos.y.raw();

			u32 xsh0 = (xr - (4 << 16)) >> (16 + 4),
				xsh1 = (xr + (4 << 16)) >> (16 + 4);

			u32 ysh0 = (yr - (4 << 16)) >> (16 + 4),
				ysh1 = (yr + (4 << 16)) >> (16 + 4);

			transient_state.worm_bloom_x |= (1 << (xsh0 & 31));
			transient_state.worm_bloom_x |= (1 << (xsh1 & 31));
			transient_state.worm_bloom_y |= (1 << (ysh0 & 31));
			transient_state.worm_bloom_y |= (1 << (ysh1 & 31));
		}
	}

	{
		auto r = this->sobjects.all();

		for (SObject* s; (s = r.next()) != 0; ) {
			if (s->time_to_die == this->current_time) {
				this->sobjects.free(r);
			}
		}
	}
	
	{
		auto r = this->nobjects.all();

		for (NObject* b; (b = r.next()) != 0; ) {
			liero::update(*b, *this, r, transient_state);
		}
	}

	{
		auto r = this->bobjects.all();

		for (BObject* b; (b = r.next()) != 0; ) {
			if (!b->update(*this)) {
				this->bobjects.free(r);
			}
		}
	}

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			w->update(*this, transient_state, this->worms.index_of(w));
		}
	}

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			w->ninjarope.update(*w, *this);
		}
	}

	++this->current_time;
}

}