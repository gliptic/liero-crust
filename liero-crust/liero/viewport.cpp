#include "viewport.hpp"

#include "../lierosim/state.hpp"
#include "blit.hpp"

namespace liero {

void draw_small_sprite(ModRef& mod, tl::ImageSlice const& clipped, u32 frame, tl::VectorI2 ipos) {

	auto sprite = mod.small_sprites.crop_square_sprite_v(frame);

	tl::BlitContext::one_source(clipped, sprite, ipos.x - 3, ipos.y - 3).blit(0, tl::ImageSlice::BlitTransparent);
}

void draw_large_sprite(ModRef& mod, tl::ImageSlice const& clipped, u32 frame, tl::VectorI2 ipos) {
	
	auto sprite = mod.large_sprites.crop_square_sprite_v(frame);

	tl::BlitContext::one_source(clipped, sprite, ipos.x, ipos.y).blit(0, tl::ImageSlice::BlitTransparent);
}

i32 muzzle_fire_offset[7][2] =
{
	{3, 1}, {4, 0}, {4, -2}, {4, -4}, {3, -5}, {2, -6}, {0, -6}
};

void Viewport::draw(State& state, DrawTarget& target, TransientState const& transient_state) {

	auto clipped = target.image.crop(this->screen_pos);

	clipped.blit(state.level.graphics.slice(), &target.pal, this->offset.x, this->offset.y);

	// Draw bonuses
	{
		auto r = state.bonuses.all();

		for (Bonus* b; (b = r.next()) != 0; ) {
			auto ipos = b->pos.cast<i32>() + this->offset;
			auto f = state.mod.tcdata->bonus_frames()[b->frame];
			draw_small_sprite(state.mod, clipped, f, ipos);
		}
	}

	// Draw sobjects
	{
		auto r = state.sobjects.all();

		for (SObject* s; (s = r.next()) != 0; ) {
			SObjectType const& ty = state.mod.get_sobject_type(s->ty_idx);

			u32 frame = ty.start_frame() + ty.num_frames() - (s->time_to_die - state.current_time) / ty.anim_delay();
			auto sprite = state.mod.large_sprites.crop_square_sprite_v(frame);

			sobj_blit(tl::BlitContext::one_source(clipped, sprite, s->pos.x, s->pos.y));
		}
	}

	// Draw nobjects
	{
		auto r = state.nobjects.all();

		for (NObject* n; (n = r.next()) != 0; ) {
			auto ipos = n->pos.cast<i32>() + this->offset;

			NObjectType const& ty = state.mod.get_nobject_type(n->ty_idx);

			if (ty.start_frame() >= 0 && (ty.kind() == NObjectKind::DType1 || ty.kind() == NObjectKind::DType2)) {
				i32 iangle = (n->cur_frame - 32) & 127;

				if (iangle > 64) ++iangle;

				i32 cur_frame = (iangle - 12) >> 3;

				if (cur_frame < 0) cur_frame = 0;
				else if (cur_frame > 12) cur_frame = 12;

				u32 frame = (u32)ty.start_frame() + cur_frame;
				draw_small_sprite(state.mod, clipped, frame, ipos);

			} else if (ty.start_frame() >= 0 && ty.kind() == NObjectKind::Steerable) {
				i32 iangle = (n->cur_frame - 32 + 4) & 127;

				i32 cur_frame = iangle >> 3;

				u32 frame = (u32)ty.start_frame() + cur_frame;
				draw_small_sprite(state.mod, clipped, frame, ipos);

			} else if (ty.start_frame() > 0) {
			
				u32 frame = (u32)ty.start_frame() + n->cur_frame;
				
				draw_small_sprite(state.mod, clipped, frame, ipos);
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
			u32 index = state.worms.index_of(w);
			auto ipos = w->pos.cast<i32>() + this->offset;

			auto& worm_transient_state = transient_state.worm_state[index];

			if (w->ninjarope.st != Ninjarope::Hidden) {
				auto anchor_pos = w->ninjarope.pos.cast<i32>() + this->offset;
				auto from_pos = ipos - tl::VectorI2(0, 1);

				draw_ninjarope(clipped, anchor_pos, from_pos,
					tl::VecSlice<tl::Color>(
						state.mod.pal.entries + state.mod.tcdata->nr_colour_begin(),
						state.mod.pal.entries + state.mod.tcdata->nr_colour_end()));

				draw_large_sprite(state.mod, clipped, 84, anchor_pos - tl::VectorI2(1, 1));
			}

			WeaponType const& ty = state.mod.get_weapon_type(w->weapons[w->current_weapon].ty_idx);

			if (ty.muzzle_fire() && w->muzzle_fire) {
				auto ang_frame = w->angle_frame();
				i32 sign_mask = -(i32)w->direction();

				auto sprite = state.mod.muzzle_fire_sprites[w->direction()].crop_square_sprite_v(ang_frame);

				muzzle_fire_blit(
					tl::BlitContext::one_source(
						clipped,
						sprite,
						(muzzle_fire_offset[ang_frame][0] ^ sign_mask) + ipos.x - 7,
						muzzle_fire_offset[ang_frame][1] + ipos.y - 5),
					state.mod.pal,
					w->muzzle_fire / 2);
			}

			{
				u32 worm_sprite = w->current_frame(state.current_time, worm_transient_state);
				auto sprite = state.mod.worm_sprites[w->direction()].crop_square_sprite_v(worm_sprite);

				worm_blit(
					tl::BlitContext::one_source(clipped, sprite, ipos.x - 7 - w->direction(), ipos.y - 5),
					index == 0 ? tl::Color(104, 104, 252) : tl::Color(104, 252, 104));
			}
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

		{
			auto sight_pos = worm.pos.cast<i32>() + (sincos(worm.abs_aiming_angle()) * 16).cast<i32>() + this->offset;

			u32 frame = 43; // TODO: TC constants. Should be 44 when sight is green.

			draw_small_sprite(state.mod, clipped, frame, sight_pos + tl::VectorI2(2, 1));
		}

		if (worm.prev_change()) {
			auto ipos = worm.pos.cast<i32>() + this->offset;

			WeaponType const& ty = state.mod.get_weapon_type(worm.weapons[worm.current_weapon].ty_idx);

			draw_text_small(clipped, state.mod.small_font_sprites.slice(), ty.name(), ipos.x + 1, ipos.y - 10);
		}
	}
}

}