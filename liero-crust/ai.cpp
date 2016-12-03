#include "ai.hpp"
#include <math.h>
#include <tl/approxmath/am.hpp>


namespace liero {

inline Scalar ang_mask(Scalar x) {
	auto mask = (1 << (16 + 7)) - 1;
	return Scalar::from_raw(x.raw() & mask);
}

inline Scalar ang_mask_signed(Scalar x) {
	return Scalar::from_raw((x.raw() << 9) >> 9);
}

inline Scalar ang_diff(Scalar a, Scalar b) {
	return ang_mask_signed(a - b);
}

void dummy_play_sound(liero::ModRef&, i16, TransientState&) {
	// Do nothing
}

struct MutationParams {
	usize start, end;

	MutationParams(usize start_init = 0, usize end_init = 0)
		: start(start_init), end(end_init) {
	}
};

static i32 evaluate(State& spare, TransientState& transient_state, State& state, tl::LcgPair& rand, Plan& plan, u32 worm_index, MutationParams params) {
	spare.copy_from(state, false);

	for (usize i = 0; i < plan.size(); ++i) {
		
		if (i >= params.start && i < params.end) {
			WormInput wi;

			Worm& target = spare.worms.of_index(worm_index ^ 1);
			Worm& worm = spare.worms.of_index(worm_index);

			if ((rand.next() % 3) == 0) {

				u8 act = (rand.next() % 6) << 4;
				u8 move = (rand.next() % 4) << 2;
				u8 aim = (rand.next() % 4);

				wi = WormInput(act, move, aim);
			} else {
				auto cur_aim = worm.abs_aiming_angle();

				auto dir = target.pos.cast<f64>() - worm.pos.cast<f64>();

				f64 radians = atan2(dir.y, dir.x);
				auto ang_to_target = ang_mask(Scalar::from_raw(i32(radians * (65536.0 * 128.0 / tl::pi2))));

				auto tolerance = Scalar(4);

				auto aim_diff = ang_diff(ang_to_target, cur_aim);

				bool fire = aim_diff >= -tolerance
					&& aim_diff <= tolerance /*
											 && obstacles(game, &worm, target) < 4*/;

				bool move_right = worm.pos.x < target.pos.x;

				bool aim_up = move_right ^ (aim_diff > Scalar());

				if (rand.next() < 0xffffffff / 120) {
					wi = WormInput::change(WormInput::Move::Left, aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
				} else {
					wi = WormInput::combo(false, fire,
						move_right ? WormInput::Move::Right : WormInput::Move::Left,
						aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
				}
			}

			plan.of_index(i) = wi;
		}

		transient_state.init(state.worms.size(), dummy_play_sound, 0);
		transient_state.graphics = true;
		transient_state.worm_state[worm_index].input = plan.of_index(i);
		spare.update(transient_state);
	}

	i32 score;
	{
		Worm& target = spare.worms.of_index(worm_index ^ 1);
		Worm& worm = spare.worms.of_index(worm_index);

		f64 distance = tl::length(target.pos.cast<f64>() - worm.pos.cast<f64>()) - 20;

		score = worm.health - target.health - (i32)abs(distance / 2.0);
	}

	return score;
}

void Ai::do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state) {

	Plan candidate(cur_plan);

	usize b = rand.get_i32((i32)candidate.storage.size());
	usize e = b + 1 + rand.get_i32(i32(candidate.storage.size() - b));

	//Worm& target = state.worms.of_index(worm_index ^ 1);

#if 0
	// TODO: Mutate during evaluation
	for (usize i = b; i < e; ++i) {
		WormInput wi;

		if ((rand() % 3) == 0) {

			u8 act = (rand() % 6) << 4;
			u8 move = (rand() % 4) << 2;
			u8 aim = (rand() % 4);

			wi = WormInput(act, move, aim);
		} else {
#if 0
			auto cur_aim = worm.abs_aiming_angle();

			auto dir = target.pos.cast<f64>() - worm.pos.cast<f64>();

			f64 radians = atan2(dir.y, dir.x);
			auto ang_to_target = ang_mask(Scalar::from_raw(i32(radians * (65536.0 * 128.0 / tl::pi2))));

			auto tolerance = Scalar(4);

			auto aim_diff = ang_diff(ang_to_target, cur_aim);

			bool fire = aim_diff >= -tolerance
				&& aim_diff <= tolerance /*
										 && obstacles(game, &worm, target) < 4*/;

			bool move_right = worm.pos.x < target.pos.x;

			bool aim_up = move_right ^ (aim_diff > Scalar());

			if (rand.next() < 0xffffffff / 120) {
				wi = WormInput::change(WormInput::Move::Left, aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
			} else {
				wi = WormInput::combo(false, fire,
					move_right ? WormInput::Move::Right : WormInput::Move::Left,
					aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
			}
#endif
		}

		candidate.of_index(i) = wi;
	}
#endif

	i32 candidate_score = evaluate(this->spare, this->spare_transient_state, state, this->rand, candidate, worm_index, MutationParams(b, e));

	if (cur_plan_score_valid_for > 0) {
		--cur_plan_score_valid_for;
	} else {
		cur_plan_score = evaluate(this->spare, this->spare_transient_state, state, this->rand, cur_plan, worm_index, MutationParams());
		cur_plan_score_valid_for = 2;
	}

	if (candidate_score >= cur_plan_score) {
		cur_plan = move(candidate);
		cur_plan_score = candidate_score;
		cur_plan_score_valid_for = 2;
	}

	transient_state.input = cur_plan.storage[cur_plan.start];
	cur_plan.storage[cur_plan.start] = cur_plan.last();
	cur_plan.shift();

#if 0
	Worm& target = state.worms.of_index(worm_index ^ 1);
	
	//auto cs = worm.controlStates;

	if (! true /*worm.visible*/) {
		//cs.set(Worm::Fire, true);
	} else {
		auto cur_aim = worm.abs_aiming_angle();

		auto dir = target.pos.cast<f64>() - worm.pos.cast<f64>();

		f64 radians = atan2(dir.y, dir.x);
		auto ang_to_target = ang_mask(Scalar::from_raw(i32(radians * (65536.0 * 128.0 / tl::pi2))));

		auto tolerance = Scalar(4);

		auto aim_diff = ang_diff(ang_to_target, cur_aim);

		bool fire = aim_diff >= -tolerance
			&& aim_diff <= tolerance /*
			&& obstacles(game, &worm, target) < 4*/;

		bool move_right = worm.pos.x < target.pos.x;

		bool aim_up = move_right ^ (aim_diff > Scalar());
			
		if (state.gfx_rand.next() < 0xffffffff / 120) {
			transient_state.input = WormInput::change(WormInput::ChangeDir::Left, aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
		} else {
			transient_state.input = WormInput::combo(false, fire,
				move_right ? WormInput::Move::Right : WormInput::Move::Left,
				aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
		}

	}
#endif
}

}
