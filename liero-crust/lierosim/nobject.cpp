#include "nobject.hpp"
#include "state.hpp"
#include <tl/approxmath/am.hpp>

namespace liero {

void NObject::explode_obj(NObjectType const& ty, Vector2 pos, Vector2 vel, State& state) {
	
	if (ty.sobj_expl_type >= 0) {
		state.mod.get_sobject_type(ty.sobj_expl_type).create(state, pos.cast<i32>());
	}
	
	u32 count = ty.splinter_count;

	if (count) {
	
		NObjectType const& splinter_ty = state.mod.get_nobject_type(ty.splinter_type);

		for (u32 i = 0; i < count; ++i) {

			Scalar angle;
			Vector2 part_vel;

			// TODO: Color is sometimes decided by spawner

			if (ty.splinter_scatter == NObjectType::ScNormal) {
			
				part_vel = vel;
				angle = Scalar(0);

			} else if (ty.splinter_scatter == NObjectType::ScAngular) {
#if LIERO_FIXED
				angle = Fixed::from_raw(state.rand.next() & ((128 << 16) - 1));
#else
				angle = state.rand.get_u01() * tl::pi2;
#endif
				Scalar speed = ty.splinter_speed; // TODO: speed_v

				part_vel = sincos(angle) * speed; // vel + 
			}

			if (ty.splinter_distribution != Scalar(0)) {

				part_vel.x += ty.splinter_distribution - rand_max(state.rand, ty.splinter_distribution * 2);
				part_vel.y += ty.splinter_distribution - rand_max(state.rand, ty.splinter_distribution * 2);
			}

			splinter_ty.create(state, angle, pos, part_vel);
		}
	}
}

bool update(NObject& self, State& state, NObjectList::range& range) {
	NObject::ObjState obj_state = NObject::Alive;
	i32 iteration = 0;

	i32 max_iteration = 1;
	auto lpos = self.pos, lvel = self.vel;

	NObjectType const& ty = state.mod.get_nobject_type(self.ty_idx);

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

		if (ty.acceleration != Scalar(0)) {
			// TODO: Also for steerables
			if (ty.type == NObjectType::DType2) {
				auto dir = sincos(Scalar((i32)self.cur_frame)) * ty.acceleration;

				lvel += dir;
				// TODO: In-flight distribution
			}
		}

		if (ty.bounce != Scalar(0)) { // TODO: Not sure this will work for real nobjects
			if (!level.is_inside(inexthoz) || level.unsafe_mat(inexthoz).dirt_rock()) {
				lvel.x *= ty.bounce;
				lvel.y *= ty.friction;
				bounced = true;
			}

			if (!level.is_inside(inextver) || level.unsafe_mat(inextver).dirt_rock()) {
				lvel.x *= ty.friction;
				lvel.y *= ty.bounce;
				bounced = true;
			}
		}

		// TODO: Blood trail for nobject

		lvel *= ty.drag; // Speed scaling for wobject

		// TODO: Nobject trail for wobject

		if (ty.collide_with_objects) {
			auto impulse = lvel * ty.blowaway;
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
			if (ty.bounce == Scalar(0)) {
				if (ty.expl_ground) {
					// TODO: Draw on map for nobject
					obj_state = NObject::Exploded;
					break;
				} else {
					lvel = Vector2();
				}
			}
			animate = false; // TODO: Nobjects are animated in this case too
			sobj_trail = ty.sobj_trail_when_hitting;
		} else {
			if (!bounced) {
				// TODO: Sobject trail for nobject
			}

			lvel.y += ty.gravity;
			animate = true;
			sobj_trail = true;
		}

		if (ty.sobj_trail_type >= 0
		 && sobj_trail
	     && (state.current_time % ty.sobj_trail_delay) == 0) {
			state.mod.get_sobject_type(ty.sobj_trail_type).create(state, ipos);
		}

		if (ty.num_frames > 0 && animate && (state.current_time & 7) == 0) { // TODO: Animation interval
			if (!ty.directional_animation || self.vel.x < -liero_eps) {
				if (self.cur_frame == 0) {
					self.cur_frame = (u32)ty.num_frames; // TODO: Shouldn't num_frames be unsigned
				} else {
					--self.cur_frame;
				}
			} else if (self.vel.x > liero_eps) {
				if (self.cur_frame >= (u32)ty.num_frames) {
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

		// TODO: Coldet with worms
		
	} // while (iteration < max_iteration)

	if (obj_state == NObject::Alive) {
		self.pos = lpos;
		self.vel = lvel;

		self.cell = state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), ipos, self.cell);

		return true;
	} else {
		//state.nobject_broadphase.swap_remove(state.nobjects.index_of(&self), );
		state.nobject_broadphase.swap_remove(CellNode::Index(state.nobjects.index_of(&self)), CellNode::Index(state.nobjects.size() - 1));
		state.nobjects.free(range);

		if (obj_state == NObject::Exploded) {
			NObject::explode_obj(ty, lpos, lvel, state);
		}

		return false;
	}
}

}