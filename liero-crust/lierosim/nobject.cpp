#include "nobject.hpp"
#include "state.hpp"
#include <tl/approxmath/am.hpp>

namespace liero {

void create(NObjectType const& self, State& state, Scalar angle, Vector2 pos, Vector2 vel, tl::Color override_color) {
	auto* obj = state.nobjects.new_object_reuse();
	obj->ty_idx = u32(&self - state.mod.nobject_types);
	obj->pos = pos;
	obj->vel = vel;

	// TODO: nobjects created with create2 have their velocity applied once immediately

	obj->cell = state.nobject_broadphase.insert(narrow<CellNode::Index>(state.nobjects.index_of(obj)), pos.cast<i32>());

	obj->time_to_die = state.current_time + self.time_to_live() - state.rand.get_i32(self.time_to_live_v());

	if (self.start_frame() >= 0) {

		// TODO: Combine NObjectKind and NObjectAnimation?
		if (self.kind() == NObjectKind::DType1 || self.kind() == NObjectKind::DType2) {

			i32 iangle = i32(angle);

			obj->cur_frame = iangle;
		} else if (self.kind() == NObjectKind::Normal) {

			// One way always starts on frame 0
			if (self.animation() != NObjectAnimation::OneWay) {
				obj->cur_frame = state.rand.get_i32(self.num_frames() + 1);
			} else {
				obj->cur_frame = 0;
			}

		} else {
			// TODO: Other types
			obj->cur_frame = 0;
		}
	} else {
		obj->cur_frame = override_color.v ?
			override_color.v : self.colors()[state.gfx_rand.get_i32(2)].v;
	}
}


void NObject::explode_obj(NObjectType const& ty, Vector2 pos, Vector2 vel, State& state) {
	
	if (ty.sobj_expl_type() >= 0) {
		create(state.mod.get_sobject_type(ty.sobj_expl_type()), state, pos.cast<i32>());
	}
	
	u32 count = ty.splinter_count();

	if (count) {
	
		NObjectType const& splinter_ty = state.mod.get_nobject_type(ty.splinter_type());

		for (u32 i = 0; i < count; ++i) {

			Scalar angle = Scalar(0);
			Vector2 part_vel = vel; // TODO: vel_ratio

			// TODO: In original, nobjects splinter into Angular and wobjects get to choose
			// Angular never inherits velocity there.

			if (ty.splinter_scatter() == ScatterType::ScAngular) {

				angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));

				Ratio speed = ty.splinter_speed(); // TODO: speed_v

				part_vel = sincos(angle) * speed;
			}

			if (ty.splinter_distribution() != Ratio()) {

				part_vel += rand_max_vector2(state.rand, ty.splinter_distribution());
			}

			create(splinter_ty, state, angle, pos, part_vel, ty.splinter_color());
		}
	}

	draw_level_effect(state, pos.cast<i32>(), ty.level_effect());
}

u32 rotl(u32 x, u32 v) {
	return (x << v) | (x >> (32 - v));
}

bool update(NObject& self, State& state, NObjectList::Range& range) {
	NObject::ObjState obj_state = NObject::Alive;
	u32 iteration = 0;

	auto lpos = self.pos, lvel = self.vel;

	NObjectType const& ty = state.mod.get_nobject_type(self.ty_idx);
	u32 max_iteration = ty.physics_speed();

	tl::VectorI2 ipos;

	while (iteration < max_iteration) {
		++iteration;

		lpos += lvel;

		auto inextpos = (lpos + lvel).cast<i32>();
		ipos = lpos.cast<i32>();
		bool bounced = false;

		// -- NObjectType things --
		Level& level = state.level;
		
		tl::VectorI2 inexthoz(inextpos.x, ipos.y);
		tl::VectorI2 inextver(ipos.x, inextpos.y);

		// TODO: Convert lvel to VectorD2 during computation with acceleration/bounce/friction/drag/trail/collide

		if (ty.acceleration() != Ratio()) {
			// TODO: Also for steerables
			if (ty.kind() == NObjectKind::DType2) {
				auto dir = sincos(Scalar((i32)self.cur_frame)) * ty.acceleration();

				lvel += dir;
				// TODO: In-flight distribution
			}
		}

		if (ty.bounce() != Ratio()) { // TODO: Not sure this will work for real nobjects
			if (!level.is_inside(inexthoz) || level.unsafe_mat(inexthoz).dirt_rock()) {
				lvel.x *= ty.bounce();
				lvel.y *= ty.friction();
				bounced = true;
			}

			if (!level.is_inside(inextver) || level.unsafe_mat(inextver).dirt_rock()) {
				lvel.x *= ty.friction();
				lvel.y *= ty.bounce();
				bounced = true;
			}
		}

		// Blood trail for nobject
		if (ty.blood_trail_interval() && (state.current_time % ty.blood_trail_interval()) == 0) {
			BObject::create(state, lpos, lvel / 4);
		}

		lvel *= ty.drag(); // Speed scaling for wobject

		// Nobject trail for wobject
		if (ty.nobj_trail_interval() && (state.current_time % ty.nobj_trail_interval()) == 0) {
			// TODO: Generalize these emitters?
			Scalar angle = Scalar(0);
			Vector2 part_vel = lvel * ty.nobj_trail_vel_ratio();

			NObjectType const& trail_ty = state.mod.get_nobject_type(ty.nobj_trail_type());

			if (ty.nobj_trail_scatter() == ScatterType::ScAngular) {

				angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));

				Ratio speed = ty.nobj_trail_speed(); // TODO: speed_v

				part_vel += sincos(angle) * speed;
			}

			create(trail_ty, state, angle, lpos, part_vel);
		}

		if (ty.collide_with_objects()) {
			auto impulse = lvel * ty.blowaway();
			i32 detect_range = 2;

			auto r = state.nobject_broadphase.area(ipos.x - detect_range, ipos.y - detect_range, ipos.x + detect_range, ipos.y + detect_range);

			for (u16 idx; r.next(idx); ) {
				auto& nobj = state.nobjects.of_index(idx);

				if (nobj.ty_idx != self.ty_idx) { // TODO: nobj.owner_idx != self.owner_idx
					auto other_pos = nobj.pos;

					if (other_pos.x <= lpos.x + Scalar(detect_range)
					 && other_pos.x >= lpos.x - Scalar(detect_range)
					 && other_pos.y <= lpos.y + Scalar(detect_range)
					 && other_pos.y >= lpos.y - Scalar(detect_range)) {
						nobj.vel += impulse;
					}
				}
			}
		}

		// lvel may have changed. Adjust inextpos
		inextpos = (lpos + lvel).cast<i32>();

		// TODO: Limit lpos if inewpos is outside the level

		bool animate, sobj_trail;

		if (!level.is_inside(inextpos) || level.unsafe_mat(inextpos).dirt_rock()) {
			if (ty.expl_ground()) {
				// TODO: Draw on map for nobject
				if (self.cur_frame >= 0 && ty.draw_on_level()) {
					// TODO: Make sure (ty.start_frame() + self.cur_frame) isn't out of range
					auto sprite = state.mod.small_sprites.crop_square_sprite_v(ty.start_frame() + self.cur_frame);

					tl::ImageSlice targets[2] = { state.level.graphics.slice(), state.level.materials.slice() };
					tl::ImageSlice sources[1] = { sprite };

					tl::BasicBlitContext<2, 1> ctx(targets, sources, ipos.x - 3, ipos.y - 3);

					draw_sprite_on_level(ctx);
				}

				obj_state = NObject::Exploded;
				break; // TODO: doing break here means we don't do worm coldet. Original would do worm coldet.
			} else if (ty.bounce() == Ratio()) {
				// TODO: NObjects do this regardless of ty.bounce()
				lvel = Vector2();
			}

			animate = false;
			sobj_trail = ty.sobj_trail_when_hitting();
		} else {
			lvel.y += ty.gravity();
			animate = true;
			sobj_trail = !bounced || ty.sobj_trail_when_bounced();
		}

		if (ty.sobj_trail_interval()
		 && sobj_trail
	     && (state.current_time % ty.sobj_trail_interval()) == 0) {
			create(state.mod.get_sobject_type(ty.sobj_trail_type()), state, ipos);
		}

		if (ty.animation() != NObjectAnimation::Static
		 && animate
		 && (state.current_time & 7) == 0) { // TODO: Animation interval

			if (ty.animation() == NObjectAnimation::OneWay || self.vel.x < Scalar()) {
				if (self.cur_frame == 0) {
					self.cur_frame = (u32)ty.num_frames(); // TODO: Shouldn't num_frames be unsigned
				} else {
					--self.cur_frame;
				}
			} else if (self.vel.x > Scalar()) {
				if (self.cur_frame >= (u32)ty.num_frames()) {
					self.cur_frame = 0;
				} else {
					++self.cur_frame;
				}
			}
		}

		if (state.current_time == self.time_to_die) {
			obj_state = NObject::Exploded;
			break;
		}

		// Coldet with worms
		if (ty.hit_damage()) {
			u32 xsh1 = (ipos.x - ty.detect_distance()) >> 4;
			u32 ysh1 = (ipos.y - ty.detect_distance()) >> 4;

			u32 maxx = ((2 * ty.detect_distance() + 15 + 15) >> 4);
			u32 maxy = ((2 * ty.detect_distance() + 15 + 15) >> 4);

			u32 xmask = (1 << (maxx & 31)) - 1;
			u32 ymask = (1 << (maxy & 31)) - 1;

			xmask = rotl(xmask, xsh1);
			ymask = rotl(ymask, ysh1);

			++state.col_mask_tests;

			if ((state.worm_bloom_x & xmask)
			 && (state.worm_bloom_y & ymask)) {
				//obj_state = NObject::Exploded;
				//break;

				++state.col_tests;
			}

			auto& w = state.worms.of_index(0);

			if (!(ipos.x + (i32)ty.detect_distance() <= i32(w.pos.x) - 4 || ipos.x - (i32)ty.detect_distance() >= i32(w.pos.x) + 4)) {
				++state.col2_tests;
			}
		}
		
	} // while (iteration < max_iteration)

	if (obj_state == NObject::Alive) {
		self.pos = lpos;
		self.vel = lvel;

		self.cell = state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), ipos, self.cell);

		return true;
	} else {
		state.nobject_broadphase.swap_remove(CellNode::Index(state.nobjects.index_of(&self)), CellNode::Index(state.nobjects.size() - 1));
		state.nobjects.free(range);

		if (obj_state == NObject::Exploded) {
			NObject::explode_obj(ty, lpos, lvel, state);
		}

		return false;
	}
}

}