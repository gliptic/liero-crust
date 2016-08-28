#include "sobject.hpp"
#include "state.hpp"

namespace liero {

void create(SObjectType const& self, State& state, tl::VectorI2 pos) {
	SObject* obj = state.sobjects.new_object_reuse();
	obj->pos = tl::VectorI2(pos.x - 8, pos.y - 8);
	obj->ty_idx = u32(&self - state.mod.sobject_types);
	obj->time_to_die = state.current_time + self.anim_delay() * (self.num_frames() + 1);

	// TODO: Damage and stuff

	Scalar blow_away = 3000_lf; // TODO: This is for large_explosion
	Scalar obj_blow_away = blow_away * (1.0 / 3.0); // TODO: Store

	if (self.detect_range()) {
		u32 ldetect_range = self.detect_range();

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

	draw_level_effect(state, pos, self.level_effect());
}


}