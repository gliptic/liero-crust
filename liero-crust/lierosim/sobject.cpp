#include "sobject.hpp"
#include "state.hpp"

namespace liero {

void create(SObjectType const& self, State& state, TransientState& transient_state, tl::VectorI2 pos) {

	if (transient_state.graphics) {
	
		SObject* obj = state.sobjects.new_object_reuse();
		obj->pos = tl::VectorI2(pos.x - 8, pos.y - 8);
		obj->ty_idx = u32(&self - state.mod.sobject_types);
		obj->time_to_die = state.current_time + self.anim_delay() * (self.num_frames() + 1);

		if (self.start_sound() >= 0) {
			i16 sound = i16(self.start_sound() + state.gfx_rand.get_i32(self.num_sounds()));
			transient_state.play_sound(state.mod, sound, transient_state);
		}
	}

	Scalar worm_blow_away = self.worm_blow_away();
	Scalar obj_blow_away = self.nobj_blow_away();

	i32 ldetect_range = self.detect_range();

	if (ldetect_range) {

		auto r = state.nobject_broadphase.area(pos.x - ldetect_range + 1, pos.y - ldetect_range + 1, pos.x + ldetect_range - 1, pos.y + ldetect_range - 1);

		for (u16 nobj_idx; r.next(nobj_idx); ) {
			auto& nobj = state.nobjects.of_index(nobj_idx);
			auto& nobj_ty = state.mod.get_nobject_type(nobj.ty_idx);
			if (nobj_ty.affect_by_sobj()) {
				auto ipos = nobj.pos.cast<i32>();

				if (ipos.x < pos.x + i32(ldetect_range)
					&& ipos.x > pos.x - i32(ldetect_range)
					&& ipos.y < pos.y + i32(ldetect_range)
					&& ipos.y > pos.y - i32(ldetect_range)) {

					auto delta = ipos - pos;
					auto power = tl::VectorI2(ldetect_range - abs(delta.x), ldetect_range - abs(delta.y));

					nobj.vel.x += obj_blow_away * power.x * sign(delta.x);
					nobj.vel.y += obj_blow_away * power.y * sign(delta.y);
				}
			}
		}
	}

	if (self.damage()) {
		
		// TODO: might_collide_with_worm assumes the worm is not a point, but this test is against a point.

		if (transient_state.might_collide_with_worm(pos, ldetect_range)) {
			++transient_state.col_tests;

			auto wr = state.worms.all();

			for (Worm* w; (w = wr.next()) != 0; ) {
				auto wpos = w->pos.cast<i32>();
				if (wpos.x < pos.x + ldetect_range
					&& wpos.x > pos.x - ldetect_range
					&& wpos.y < pos.y + ldetect_range
					&& wpos.y > pos.y - ldetect_range) {

					auto delta = wpos - pos;
					auto power = tl::VectorI2(ldetect_range - abs(delta.x), ldetect_range - abs(delta.y));

					if (fabs(w->vel.x) < Scalar(2)) {
						w->vel.x += worm_blow_away * power.x * sign(delta.x);
					}

					if (fabs(w->vel.y) < Scalar(2)) {
						w->vel.y += worm_blow_away * power.y * sign(delta.y);
					}

					u32 ldamage = self.damage() * u32(power.x + power.y) / 2;
					if (ldetect_range)
						ldamage /= ldetect_range;

					// TODO: Cause damage to worm
					w->do_damage(ldamage);
				}
			}
		}
	}

	draw_level_effect(state, pos, self.level_effect(), transient_state.graphics, transient_state);
}


}