#ifndef TEST_CELLPHASE_HPP
#define TEST_CELLPHASE_HPP 1

#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/texture.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/rand.h>
#include <tl/io/stream.hpp>
#include <memory>

#include "lierosim/state.hpp"
#include "liero/viewport.hpp"
#include <tl/windows/miniwindows.h>
#include <liero-sim/config.hpp>
#include <tl/cstdint.h>

#include "lierosim/object_list.hpp"

namespace test_cellphase {

using namespace liero;

static int const ObjLimit = 2 * (600 + 600);

struct ObjType {
	ObjType() {
	}

	ObjType(Ratio bounce, Ratio friction, u32 time_to_live, u32 time_to_live_v)
		: bounce(bounce), friction(friction)
		, time_to_live(time_to_live)
		, time_to_live_v(time_to_live_v) {

		this->splinter_count = 0;
		this->splinter_speed = 3.0;
		this->splinter_speed_v = 0;
		this->splinter_type = 0;
	}

	Ratio bounce, friction;
	u32 time_to_live, time_to_live_v;
	u32 splinter_count;
	u32 splinter_type;
	Ratio splinter_speed, splinter_speed_v;
};


struct BaseState {
	liero::Level level;
	u32 frame;
	ObjType obj_types[2];

	BaseState()
		: frame(0) {

		obj_types[0] = ObjType(-0.4, 1.0, 100, 30);
		obj_types[0].splinter_count = 100;
		obj_types[0].splinter_type = 1;
		obj_types[1] = ObjType(-0.4, 1.0, 200, 100);
	}
};

inline u32 rotl(u32 v, u32 d) {
	return (v << d) | (v >> (32-d));
}

struct Token {
	u32 bits;

	static const u32 frame_bits = 18;
	static const u32 index_mult = (1433 << frame_bits);
	//static const u32 index_mult = (8483 << frame_bits);
	static const u32 frame_mask = (1 << frame_bits) - 1;
	static const u32 index_bits = 32 - frame_bits;

	Token() {
	}

	Token(u32 frame, u32 index)
		: bits((frame & frame_mask) | (index * (2 * index + 1) * index_mult)) {
		//: bits((frame & frame_mask) | (index * index_mult)) {
	}

	u32 frame() {
		return this->bits & frame_mask;
	}

	u32 elapsed(u32 current_frame) {
		// Assumes current_frame is same or later frame
		return (current_frame - this->bits) & frame_mask;
	}

	tl::LcgPair get_seed(u32 salt) {
		//u32 a = rotl(this->bits ^ salt, 0) * 2654435761;
		u32 a = (this->bits ^ salt) * 2654435761;
		u32 b = (a ^ salt)*2654435761;
		return tl::LcgPair(a, b);
	}


};

enum class ObjState {
	Alive,
	Removed,
	Exploded
};


template<typename T, typename St>
void explode_obj(T& ty, Vector2 pos, Vector2 vel, St& state, Token token) {
	auto count = ty.splinter_count;
	if (count) {
		ObjType const& splinter_ty = state.obj_types[ty.splinter_type];

		auto rand = token.get_seed(0x69e34414);

		for (u32 i = 0; i < count; ++i) {

			Scalar angle = Scalar(0);
			Vector2 part_vel = vel; // TODO: vel_ratio

			// TODO: In original, nobjects splinter into Angular and wobjects get to choose.
			// Angular never inherits velocity there.

			//if (ty.splinter_scatter() == ScatterType::ScAngular) {

			angle = Fixed::from_raw(rand.next() & ((128 << 16) - 1));

			Ratio speed = ty.splinter_speed + rand.get_f64(ty.splinter_speed_v);

			part_vel = vector2(sincos_f64(angle) * speed);
			//}

			//if (ty.splinter_distribution() != Ratio()) {

			//	part_vel += rand_max_vector2(state.rand, ty.splinter_distribution());
			//}

			//state.create(splinter_ty, state, angle, pos, part_vel, owner, ty.splinter_color());
			state.create(pos, part_vel, &splinter_ty - state.obj_types);
		}
	}
}


template<typename T, typename St>
ObjState update(T& self, St& state) {
	auto lpos = self.pos, lvel = self.vel;

	lpos += lvel;
	auto ipos = lpos.cast<i32>();

	Level& level = state.level;
	tl::VectorD2 fvel(lvel.x.raw(), lvel.y.raw());
	auto& ty = state.obj_types[self.ty_idx];

	if (ty.bounce != Ratio()) { // TODO: Not sure this will work for real nobjects
								// TODO: This should probably use the modified fvel
		tl::VectorI2 inextpos = (lpos + lvel).cast<i32>();
		tl::VectorI2 inexthoz(inextpos.x, ipos.y);
		tl::VectorI2 inextver(ipos.x, inextpos.y);

		if (level.mat_dirt_rock(inexthoz)) {
			fvel.x *= ty.bounce;
			fvel.y *= ty.friction;
			//bounced = true;
		}

		if (level.mat_dirt_rock(inextver)) {
			fvel.x *= ty.friction;
			fvel.y *= ty.bounce;
			//bounced = true;
		}
	}

	//fvel *= ty.drag(); // Speed scaling for wobject
	lvel = vector2(fvel);

	lvel.y += Scalar::from_raw(1500);

	if (state.obj_timed_out(self)) {
		return ObjState::Exploded;
	}

	self.pos = lpos;
	self.vel = lvel;
	return ObjState::Alive;
}

}

#endif // TEST_CELLPHASE_HPP
