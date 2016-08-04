#include "state.hpp"

#include "../gfx/buttons.hpp"

namespace liero {

void State::update(gfx::CommonWindow& window) {

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
			liero::update(*b, *this, r);
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

	StateInput input;

	auto& c = input.worm_controls[0];
	c.set(Worm::Left, window.button_state[kbLeft] != 0);
	c.set(Worm::Right, window.button_state[kbRight] != 0);
	c.set(Worm::Up, window.button_state[kbUp] != 0);
	c.set(Worm::Down, window.button_state[kbDown] != 0);
	c.set(Worm::Fire, window.button_state[kbS] != 0);
	c.set(Worm::Jump, window.button_state[kbR] != 0);
	c.set(Worm::Change, window.button_state[kbA] != 0);

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			w->update(*this, input);
		}
	}

	++this->current_time;
}

}