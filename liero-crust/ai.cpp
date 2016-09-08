#include "lierosim/state.hpp"
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

void do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state) {

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
}

}
