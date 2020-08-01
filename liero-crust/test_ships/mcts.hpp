#pragma once

#include "test_ships.hpp"
#include <tl/rand.hpp>

namespace test_ships {

struct Bot {
	virtual State::Controls move(State& state) = 0;
};

State::Controls random_move(tl::LcgPair& rand, State::Controls prev_controls) {
	auto new_controls = prev_controls;
	if (rand.next() < 0xffffffff / 5) {
		new_controls ^= State::c_forward;
	}
	if (rand.next() < 0xffffffff / 10) {
		new_controls ^= State::c_back;
	}
	if (rand.next() < 0xffffffff / 7) {
		new_controls ^= State::c_aim_left;
	}
	if (rand.next() < 0xffffffff / 7) {
		new_controls ^= State::c_aim_right;
	}
	if (rand.next() < 0xffffffff / 5) {
		new_controls ^= State::c_left;
	}
	if (rand.next() < 0xffffffff / 5) {
		new_controls ^= State::c_right;
	}
	if (rand.next() < 0xffffffff / 3) {
		new_controls ^= State::c_fire;
	}

	return new_controls;
}

struct SillyBot : Bot {
	State::Controls prev_controls;
	tl::LcgPair rand;

	SillyBot()
		: prev_controls(0)
		, rand(0, 0) {
	}

	State::Controls move(State& state) override {
		return random_move(rand, prev_controls);
	}
};

struct PredBot : Bot {
	State::Controls prev_controls;
	tl::Vec<State::Controls> plan;
	double plan_value;
	tl::LcgPair rand;

	PredBot()
		: rand(0, 0), plan_value(-1000000.0), prev_controls() {
	}

	double value(State const& state) {
		//return -tl::length((state.tanks[1].pos - state.tanks[0].pos).cast<f64>());
		if (state.tanks[1].hp == 0) return -100000.0;
		if (state.tanks[0].hp == 0) return 100000.0;
		return state.tanks[1].hp - state.tanks[0].hp;
	}

	double eval(State::Controls pc, tl::Vec<State::Controls>& plan, State const& state) {
		State test_state;
		test_state.copy_from(state);
		State::TransientState ts;
		ts.tank_controls[0] = State::Controls();

		for (u32 i = 0; i < 60; ++i) {
			if (i >= plan.size()) {
				plan.push_back(pc = random_move(rand, pc));
			} else {
				pc = plan[i];
			}

			ts.tank_controls[1] = pc;

			test_state.process(ts);
		}

		return value(test_state);
	}

	State::Controls move(State& state) override {
		plan_value = eval(prev_controls, plan, state);

		tl::Vec<State::Controls> new_plan;
		double new_plan_value = eval(prev_controls, new_plan, state);

		if (new_plan_value > plan_value) {
			this->plan = std::move(new_plan);
			printf("%f\n", new_plan_value);
		}

		auto action = this->plan[0];
		prev_controls = action;
		this->plan.pop_front();
		return action;
	}
};

struct Node {

};

struct Mcts {
	
};

}
