#ifndef LIERO_AI_HPP
#define LIERO_AI_HPP 1

#include "lierosim/state.hpp"

namespace liero {


struct Plan {
	Plan(usize s)
		: start(0) {

		for (usize i = 0; i < s; ++i) {
			storage.push_back(WormInput());
		}
	}

	Plan& operator=(Plan&& other) {
		this->start = other.start;
		this->storage = move(other.storage);
		return *this;
	}

	explicit Plan(Plan const& other)
		: start(other.start)
		, storage(other.storage.slice_const()) {
	}

	u32 start;
	tl::Vec<WormInput> storage;

	void shift() {
		if (++start == storage.size()) {
			start = 0;
		}
	}

	WormInput& last() {
		usize s = storage.size();
		return storage[(start + s - 1) % s];
	}

	WormInput& of_index(usize idx) {
		return storage[(start + idx) % storage.size()];
	}

	usize size() {
		return storage.size();
	}
};

struct Ai {

	State spare;
	TransientState spare_transient_state;
	Plan cur_plan;
	tl::LcgPair rand;

	i32 cur_plan_score;
	i32 cur_plan_score_valid_for;

	Ai(ModRef mod)
		: spare(mod)
		, cur_plan(120)
		, rand(1, 1)
		, cur_plan_score_valid_for(0) {
	}

	void do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state);
};

}

#endif // LIERO_AI_HPP
