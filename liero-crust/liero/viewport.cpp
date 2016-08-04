#include "viewport.hpp"

#include "../lierosim/state.hpp"
#include "blit.hpp"

namespace liero {

void Viewport::draw(State& state, DrawTarget& target) {

	auto clipped = target.image.crop(this->screen_pos);

	clipped.blit(state.level.graphics.slice(), &target.pal, this->offset.x, this->offset.y);

	// Draw sobjects
	{
		auto r = state.sobjects.all();

		for (SObject* s; (s = r.next()) != 0; ) {
			SObjectType const& ty = state.mod.get_sobject_type(s->ty_idx);

			u32 frame = ty.start_frame + ty.num_frames - (s->time_to_die - state.current_time) / ty.anim_delay;

			u32 y = frame * 16;
			auto sprite = state.mod.large_sprites.crop(tl::RectU(0, y, 16, y + 16));

			sobj_blit(tl::BlitContext::one_source(clipped, sprite, s->pos.x, s->pos.y));
		}
	}

	// Draw nobjects
	{
		auto r = state.nobjects.all();

		for (NObject* n; (n = r.next()) != 0; ) {
			auto ipos = n->pos.cast<i32>() + this->offset;

			NObjectType const& ty = state.mod.get_nobject_type(n->ty_idx);

			if (ty.start_frame > 0 && (ty.type == NObjectType::DType1 || ty.type == NObjectType::DType2)) {
				i32 iangle = (n->cur_frame - 32) & 127;

				if (iangle > 64) ++iangle;

				int cur_frame = (iangle - 12) >> 3;

				if (cur_frame < 0) cur_frame = 0;
				else if (cur_frame > 12) cur_frame = 12;

				u32 y = ((u32)ty.start_frame + cur_frame) * 7;
				auto sprite = state.mod.small_sprites.crop(tl::RectU(0, y, 7, y + 7));
				tl::BlitContext::one_source(clipped, sprite, ipos.x - 3, ipos.y - 3).blit(0, tl::ImageSlice::BlitTransparent);

			} else if (ty.start_frame > 0) {
			
				u32 frame = (u32)ty.start_frame + n->cur_frame;
				u32 y = frame * 7;
				auto sprite = state.mod.small_sprites.crop(tl::RectU(0, y, 7, y + 7));
			
				tl::BlitContext::one_source(clipped, sprite, ipos.x - 3, ipos.y - 3).blit(0, tl::ImageSlice::BlitTransparent);
			} else {
				if (clipped.is_inside(ipos)) {
					clipped.unsafe_pixel32(ipos.x, ipos.y) = n->cur_frame;
				}
			}
		}
	}

	// Draw worms
	{
		auto r = state.worms.all();

		for (Worm* w; (w = r.next()) != 0; ) {
			auto ipos = w->pos.cast<i32>() + this->offset;

			u32 worm_sprite = w->current_frame(state.current_time);
			u32 y = worm_sprite * 16;

			//auto sprite = state.mod.large_sprites.crop(tl::RectU(0, y, 16, y + 16));
			auto sprite = state.mod.worm_sprites[w->direction < 0 ? 1 : 0].crop(tl::RectU(0, y, 16, y + 16));

			worm_blit(
				tl::BlitContext::one_source(clipped, sprite, ipos.x - 7 - (w->direction < 0 ? 1 : 0), ipos.y - 5));
		}
	}

	// Draw bobjects
	{
		auto r = state.bobjects.all();

		for (BObject* b; (b = r.next()) != 0; ) {
			auto ipos = b->pos.cast<i32>() + this->offset;

			if ((ipos.x >= 0 && ipos.x < (i32)this->width)
			 && (ipos.y >= 0 && ipos.y < (i32)this->height)) {
			
				clipped.unsafe_pixel32(ipos.x, ipos.y) = b->color;
			}
		}
	}

	Worm& worm = state.worms.of_index(this->worm_idx);

	if (true) { // TODO: worm && worm->visible
		// TODO: Draw sight

		if (worm.control_flags[Worm::Change]) {
			auto ipos = worm.pos.cast<i32>() + this->offset;

			WeaponType const& ty = state.mod.get_weapon_type(worm.weapons[worm.current_weapon].ty_idx);

			draw_text_small(clipped, state.mod.small_font_sprites.slice(), ty.name, ipos.x + 1, ipos.y - 10);
		}
	}
}

}