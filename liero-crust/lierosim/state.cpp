#include "state.hpp"

#include "../gfx/buttons.hpp"

namespace liero {

void State::update(TransientState& transient_state) {

	// Prepare transient state
	transient_state.worm_bloom_x = 0;
	transient_state.worm_bloom_y = 0;

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			u32 xr = w->pos.x.raw(),
				yr = w->pos.y.raw();

			u32 xsh0 = (xr - (4 << 16)) >> (16 + 4),
				xsh1 = (xr + (4 << 16)) >> (16 + 4);

			u32 ysh0 = (yr - (4 << 16)) >> (16 + 4),
				ysh1 = (yr + (4 << 16)) >> (16 + 4);

			transient_state.worm_bloom_x |= (1 << (xsh0 & 31));
			transient_state.worm_bloom_x |= (1 << (xsh1 & 31));
			transient_state.worm_bloom_y |= (1 << (ysh0 & 31));
			transient_state.worm_bloom_y |= (1 << (ysh1 & 31));
		}
	}

	transient_state.compute_current_time_modulos(this->current_time);


#if 1
	{
		// This is here to include objects created outside update. It should only
		// be necessary for testing purposes.
		auto new_nobjs = this->nobjects.flush_new_queue([&](NObject* nobj, u32 index) {
			//if (nobj->cell < 0) // TEMP DISABLE
			this->nobject_broadphase.remove(narrow<CellNode::Index>(index));
		});

		for (auto& nobj : new_nobjs) {
			assert(nobj.cell >= 0);
			i16 obj_index = narrow<CellNode::Index>(this->nobjects.index_of(&nobj));
#if !IMPLICIT_NOBJ_CELL
			nobj.cell = this->nobject_broadphase.insert(obj_index, nobj.pos);
#else
			this->nobject_broadphase.insert(obj_index, nobj.pos);
#endif
		}

		this->nobjects.reset_new_queue();
	}
#endif

	// Updates

	{
		auto r = this->sobjects.all();

		for (SObject* s; (s = r.next()) != 0; ) {
			if (s->time_to_die == this->current_time) {
				this->sobjects.free(r);
			}
		}
	}
	
	{
		auto r = this->nobjects.all();

		for (NObject* b; (b = r.next()) != 0; ) {
			liero::update(*b, *this, r, transient_state);
		}
	}

	{
		auto r = this->bobjects.all();

		for (BObject* b; (b = r.next()) != 0; ) {
			if (!b->update(*this)) {
				this->bobjects.free(r);
			}
		}
	}

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			w->update(*this, transient_state, this->worms.index_of(w));
		}
	}

	{
		auto r = this->worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			w->ninjarope.update(*w, *this);
		}
	}

	{
		auto new_nobjs = this->nobjects.flush_new_queue([&](NObject* nobj, u32 index) {
			//if (nobj->cell < 0) // TEMP DISABLE
			this->nobject_broadphase.remove(narrow<CellNode::Index>(index));
		});

		for (auto& nobj : new_nobjs) {
			assert(nobj.cell >= 0);
			i16 obj_index = narrow<CellNode::Index>(this->nobjects.index_of(&nobj));
#if !IMPLICIT_NOBJ_CELL
			nobj.cell = this->nobject_broadphase.insert(obj_index, nobj.pos);
#else
			this->nobject_broadphase.insert(obj_index, nobj.pos);
#endif
		}
	}

#if QUEUE_REMOVE_NOBJS
	{
		usize queue_size = transient_state.nobj_remove_queue_ptr - transient_state.nobj_remove_queue;
		for (usize i = queue_size; i-- > 0; ) {
			auto nobj_idx = transient_state.nobj_remove_queue[i];
			u32 last = u32(this->nobjects.size() - 1);
			assert(usize(nobj_idx) < this->nobjects.size());
			assert(last == u32(nobj_idx) || this->nobjects.of_index(last).cell < 0);
			this->nobject_broadphase.move_last(CellNode::Index(last), nobj_idx);
			this->nobjects.free(&this->nobjects.of_index((u32)nobj_idx));
		}
		transient_state.nobj_remove_queue_ptr = transient_state.nobj_remove_queue;
	}
#endif

	this->nobjects.reset_new_queue();

	transient_state.apply_queues(*this);

	++this->current_time;

#if 0
	if (!level.validate_mat()) {
		printf("Invalid material bitmap in State::update!\n");
	}
#endif
}

}