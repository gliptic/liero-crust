#include "state.hpp"

#include "../gfx/buttons.hpp"

namespace liero {

#if 1
void spawn_bonus(State& state) {
	if (state.bonuses.is_full())
		return;

	auto* bonus = state.bonuses.new_object();

	auto ipos = state.rand.get_vectori2(state.level.graphics.dimensions().cast<i32>());

	u32 frame = state.rand.get_i32(2);

	bonus->pos = ipos.cast<Scalar>();
	bonus->frame = frame;
	bonus->vel_y = Scalar();
	bonus->timer = state.mod.tcdata->bonus_rand_timer_min()[frame] + state.rand.get_i32(i32(state.mod.tcdata->bonus_rand_timer_var()[frame]));
	bonus->weapon = 0;
}
#endif

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
		auto new_nobjs = this->nobjects.flush_new_queue([&](NObject*, u32 index) {
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
		auto r = this->bonuses.all();

		for (Bonus* s; (s = r.next()) != 0; ) {
			if (!liero::update(*this, *s, transient_state)) {
				this->bonuses.free(r);
			}
		}

		if (this->rand.get_i32(this->mod.tcdata->bonus_drop_chance()) == 0) {
			spawn_bonus(*this);
		}
	}

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
			NObject::ObjState new_state = liero::update(*b, *this, r, transient_state);

			if (new_state != NObject::Alive) {

				auto owner = b->owner;
				auto lpos = b->pos, lvel = b->vel;
				NObjectType const& ty = this->mod.get_nobject_type(b->ty_idx);

#if QUEUE_REMOVE_NOBJS
				CellNode::Index idx = (CellNode::Index)state.nobjects.index_of(&self);
				state.nobject_broadphase.remove(idx);
				//transient_state.nobj_remove_queue.push_back(idx);
				*transient_state.nobj_remove_queue_ptr++ = idx;
				self.cell = 0;
#else
				this->nobject_broadphase.swap_remove(CellNode::Index(this->nobjects.index_of(b)), CellNode::Index(this->nobjects.size() - 1));
				this->nobjects.free(r);
#endif

				if (new_state == NObject::Exploded) {
					NObject::explode_obj(ty, lpos, lvel, owner, *this, transient_state);
				}
			} else {

#if UPDATE_POS_IMMEDIATE
#  if !IMPLICIT_NOBJ_CELL
				b->cell = this->nobject_broadphase.update(narrow<CellNode::Index>(this->nobjects.index_of(b)), b->pos, b->cell);
#  else
				state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), ipos, self.pos);
				self.pos = lpos;
#  endif
#else
				auto idx = narrow<CellNode::Index>(state.nobjects.index_of(&self));
				transient_state.next_nobj_pos[idx].pos = lpos;
				//transient_state.next_nobj_pos[idx].cur_cell = self.cell;
#endif
			}
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
		auto new_nobjs = this->nobjects.flush_new_queue([&](NObject*, u32 index) {
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