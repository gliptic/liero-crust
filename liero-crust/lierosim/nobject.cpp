#include "nobject.hpp"
#include "state.hpp"
#include <tl/approxmath/am.hpp>

namespace liero {

void create(NObjectType const& self, State& state, Scalar angle, Vector2 pos, Vector2 vel, TransientState& transient_state, i16 owner, tl::Color override_color) {

	auto* obj = state.nobjects.new_object_reuse_queue();

	obj->ty_idx = tl::narrow<u16>(&self - state.mod.nobject_types);
	obj->pos = pos;
	obj->vel = vel;
	obj->cell = 0;

	// TODO: nobjects created with create2 have their velocity applied once immediately

	obj->owner = owner;

	obj->time_to_die = state.current_time + self.time_to_live() - state.rand.get_i32(self.time_to_live_v());

	if (self.start_frame() >= 0) {

		// TODO: Combine NObjectKind and NObjectAnimation?
		if (self.kind() == NObjectKind::DType1 || self.kind() == NObjectKind::DType2 || self.kind() == NObjectKind::Steerable) {

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


void NObject::explode_obj(NObjectType const& ty, Vector2 pos, Vector2 vel, i16 owner, State& state, TransientState& transient_state) {
	
	if (ty.sobj_expl_type() >= 0) {
		create(state.mod.get_sobject_type(ty.sobj_expl_type()), state, transient_state, pos.cast<i32>());
	}

	transient_state.play_sound(state.mod, ty.expl_sound(), transient_state);
	
	u32 count = ty.splinter_count();

	if (count) {
	
		NObjectType const& splinter_ty = state.mod.get_nobject_type(ty.splinter_type());

		for (u32 i = 0; i < count; ++i) {

			Scalar angle = Scalar(0);
			Vector2 part_vel = vel; // TODO: vel_ratio

			// TODO: In original, nobjects splinter into Angular and wobjects get to choose.
			// Angular never inherits velocity there.

			if (ty.splinter_scatter() == ScatterType::ScAngular) {

				angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));

				Ratio speed = ty.splinter_speed() - state.rand.get_i32(ty.splinter_speed_v());

				part_vel = vector2(sincos_f64(angle) * speed);
			}

			if (ty.splinter_distribution() != Ratio()) {

				part_vel += rand_max_vector2(state.rand, ty.splinter_distribution());
			}

			create(splinter_ty, state, angle, pos, part_vel, transient_state, owner, ty.splinter_color());
		}
	}

	draw_level_effect(state, pos.cast<i32>(), ty.level_effect(), transient_state.graphics, transient_state);
}

bool update(NObject& self, State& state, NObjectList::Range& range, TransientState& transient_state) {
	NObject::ObjState obj_state = NObject::Alive;
	u32 iteration = 0;

	auto lpos = self.pos, lvel = self.vel;

	NObjectType const& ty = state.mod.get_nobject_type(self.ty_idx);
	u32 max_iteration = ty.physics_speed();

	tl::VectorI2 ipos;

	do {
		++iteration;

		lpos += lvel;

		ipos = lpos.cast<i32>();
		bool bounced = false;

		tl::VectorD2 fvel(lvel.x.raw(), lvel.y.raw());

		Level& level = state.level;

		// TODO: Convert lvel to VectorD2 during computation with acceleration/bounce/friction/drag/trail/collide

		if (ty.acceleration() != Ratio()) {

			f64 acc;
			if (ty.kind() == NObjectKind::Steerable
				&& self.owner >= 0
				&& transient_state.worm_state[self.owner].input.up()) {
				acc = ty.acceleration_up();
			} else {
				acc = ty.acceleration();
			}

			auto dir = sincos_f64(Scalar((i32)self.cur_frame)) * acc;
			fvel += dir;

			// TODO: In-flight distribution
		}

		if (ty.bounce() != Ratio()) { // TODO: Not sure this will work for real nobjects
			// TODO: This should probably use the modified fvel
			tl::VectorI2 inextpos = (lpos + lvel).cast<i32>();
			tl::VectorI2 inexthoz(inextpos.x, ipos.y);
			tl::VectorI2 inextver(ipos.x, inextpos.y);

			if (level.mat_dirt_rock(inexthoz)) {
				fvel.x *= ty.bounce();
				fvel.y *= ty.friction();
				bounced = true;
			}

			if (level.mat_dirt_rock(inextver)) {
				fvel.x *= ty.friction();
				fvel.y *= ty.bounce();
				bounced = true;
			}
		}

		fvel *= ty.drag(); // Speed scaling for wobject
		lvel = vector2(fvel);

		// Blood trail for nobject
		if (transient_state.graphics
		 && ty.blood_trail_interval()
		 && (transient_state.current_time % ty.blood_trail_interval()) == 0) {
			BObject::create(state, lpos, lvel / 4);
		}

		// Nobject trail for wobject
		if (ty.nobj_trail_interval()
#if 1
		 && transient_state.is_current_time_modulo_zero(ty.nobj_trail_interval())) {
#else
		 && transient_state.current_time * ty.nobj_trail_interval_inv() < ty.nobj_trail_interval_inv()) {
#endif
			// TODO: Generalize these emitters?
			Scalar angle = Scalar(0);
			Vector2 part_vel = vector2(fvel * ty.nobj_trail_vel_ratio());

			NObjectType const& trail_ty = state.mod.get_nobject_type(ty.nobj_trail_type());

			if (ty.nobj_trail_scatter() == ScatterType::ScAngular) {

				angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));

				Ratio speed = ty.nobj_trail_speed(); // TODO: speed_v

				part_vel += vector2(sincos_f64(angle) * speed);
			}

			create(trail_ty, state, angle, lpos, part_vel, transient_state);
		}

		if (ty.collide_with_objects()) {
			auto impulse = vector2(fvel * ty.blowaway());

			i32 detect_range = 2;

			auto r = state.nobject_broadphase.area(ipos.x - detect_range, ipos.y - detect_range, ipos.x + detect_range, ipos.y + detect_range);

			for (u16 idx; r.next(idx); ) {
				auto& nobj = state.nobjects.of_index(idx);

				if (nobj.ty_idx != self.ty_idx || nobj.owner != self.owner) {
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

		// TODO: Limit lpos if inewpos is outside the level

		bool sobj_trail;

		if (level.mat_dirt_rock((lpos + lvel).cast<i32>())) {
			if (ty.expl_ground()) {

				if (self.cur_frame >= 0 && ty.draw_on_level()) {
					// TODO: Make sure (ty.start_frame() + self.cur_frame) isn't out of range
					auto sprite = state.mod.small_sprites.crop_square_sprite_v(ty.start_frame() + self.cur_frame);

					tl::ImageSlice targets[1] = { state.level.graphics.slice() };
					tl::ImageSlice sources[1] = { sprite };

					tl::BasicBlitContext<1, 1, true> ctx(targets, sources, ipos.x - 3, ipos.y - 3);

					draw_sprite_on_level(ctx, state.level.mat_iter());
				}

				obj_state = NObject::Exploded;
				break; // TODO: Doing break here means we don't do worm coldet. Original would do worm coldet for wobjects, but not for nobjects.
			} else if (ty.bounce() == Ratio()) {
				// TODO: NObjects do this regardless of ty.bounce()
				lvel = Vector2();
			}

			sobj_trail = ty.sobj_trail_when_hitting();
		} else {
			lvel.y += ty.gravity();
			sobj_trail = !bounced || ty.sobj_trail_when_bounced();

			if (ty.animation() != NObjectAnimation::Static
				&& (transient_state.current_time & 7) == 0) { // TODO: Animation interval

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
		}

		if (ty.sobj_trail_interval()
		 && sobj_trail
	     && transient_state.is_current_time_modulo_zero(ty.sobj_trail_interval())) {
			create(state.mod.get_sobject_type(ty.sobj_trail_type()), state, transient_state, ipos);
		}

		if (transient_state.current_time == self.time_to_die) {
			obj_state = NObject::Exploded;
			break; // TODO: Doing break here means we don't do worm coldet. Original would do worm coldet for wobjects, but not for nobjects.
		}

		// Coldet with worms
		if (ty.worm_coldet()) {

			if (transient_state.might_collide_with_worm(ipos, (i32)ty.detect_distance())) {
				++transient_state.col_tests;

				auto wr = state.worms.all();

				for (Worm* w; (w = wr.next()) != 0; ) {
					auto wpos = w->pos.cast<i32>();

					if (wpos.x - 2 < ipos.x + (i32)ty.detect_distance() // TODO: detect_distance() should probably be i32
					 && wpos.x + 2 > ipos.x - (i32)ty.detect_distance()
					 && wpos.y - 4 < ipos.y + (i32)ty.detect_distance()
					 && wpos.y + 4 > ipos.y - (i32)ty.detect_distance()) {

						// TODO:
						w->vel += lvel * ty.blowaway();
						w->do_damage(ty.hit_damage());

						u32 blood_amount = ty.worm_col_blood();
						for (u32 i = 0; i < blood_amount; ++i) {
							auto angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));
							auto part_vel = sincos(angle); // TODO: Correct blood velocity
							create(state.mod.get_nobject_type(40 + 6), state, angle, lpos, part_vel + lvel / 3, transient_state, self.owner);
						}

						// TODO: Play hurt sound if hit_damage > 0 etc.
						
						if (state.rand.next() <= ty.worm_col_remove_prob()) {
							obj_state = ty.worm_col_expl() ? NObject::Exploded : NObject::Removed;
						}
					}
				}

				if (obj_state != NObject::Alive) {
					break;
				}
			}

#if 0
			{
				auto& w = state.worms.of_index(0);

				if (!(ipos.x + (i32)ty.detect_distance() <= i32(w.pos.x) - 4 || ipos.x - (i32)ty.detect_distance() >= i32(w.pos.x) + 4)) {
					++transient_state.col2_tests;
				}
			}
#endif
		}
		
	} while (iteration < max_iteration);

	if (obj_state == NObject::Alive) {
		self.vel = lvel;

#if UPDATE_POS_IMMEDIATE
#if !IMPLICIT_NOBJ_CELL
		self.pos = lpos;
		self.cell = state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), lpos, self.cell);
#else
		state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), ipos, self.pos);
		self.pos = lpos;
#endif
#else
		auto idx = narrow<CellNode::Index>(state.nobjects.index_of(&self));
		transient_state.next_nobj_pos[idx].pos = lpos;
		//transient_state.next_nobj_pos[idx].cur_cell = self.cell;
#endif

		return true;
	} else {
		auto owner = self.owner;
#if QUEUE_REMOVE_NOBJS
		CellNode::Index idx = (CellNode::Index)state.nobjects.index_of(&self);
		state.nobject_broadphase.remove(idx);
		//transient_state.nobj_remove_queue.push_back(idx);
		*transient_state.nobj_remove_queue_ptr++ = idx;
		self.cell = 0;
#else
		state.nobject_broadphase.swap_remove(CellNode::Index(state.nobjects.index_of(&self)), CellNode::Index(state.nobjects.size() - 1));
		state.nobjects.free(range);
#endif

		if (obj_state == NObject::Exploded) {
			NObject::explode_obj(ty, lpos, lvel, owner, state, transient_state);
		}

		return false;
	}
}

}
