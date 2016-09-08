#include "level.hpp"

#include "mod.hpp"
#include "state.hpp"

namespace liero {

Level::Level(tl::Source& src, tl::Palette& pal, ModRef& mod) {

	this->graphics.alloc_uninit(504, 350, 1);
	this->materials.alloc_uninit(504, 350, 1);

	src.ensure(this->graphics.size());
	memcpy(this->graphics.data(), src.begin(), this->graphics.size());

	u8* mat = this->materials.data();
	u8* index_bitmap = this->graphics.data();
	for (usize i = 0; i < this->graphics.size(); ++i) {
		mat[i] = mod.materials[index_bitmap[i]]; // mat_from_index[index_bitmap[i]];
	}

	this->graphics = this->graphics.convert(4, &pal);
}

void draw_level_effect(State& state, tl::VectorI2 pos, i16 level_effect) {

	if (level_effect >= 0) {
		auto& effect = state.mod.get_level_effect(u32(level_effect));

		u32 tframe_idx = effect.sframe();

		auto tframe = state.mod.large_sprites.crop_square_sprite_v(tframe_idx);
		auto mframe = state.mod.large_sprites.crop_square_sprite_v(effect.mframe());

		auto graphics_slice = state.level.graphics.slice().crop_bottom(1);
		auto materials_slice = state.level.materials.slice().crop_bottom(1);

		tl::ImageSlice targets[2] = { graphics_slice, materials_slice };
		tl::ImageSlice sources[1] = { mframe };

		tl::BasicBlitContext<2, 1, true> ctx(targets, sources, pos.x - 7, pos.y - 7);

		draw_level_effect(ctx, tframe.pixels, effect.draw_back());
	}
}

void draw_level_effect(tl::BasicBlitContext<2, 1, true> ctx, u8 const* tframe, bool draw_on_background) {

	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* tpmat = ctx.targets[1].pixels;
	u8* fp = ctx.sources[0].pixels;

	u32 tpitch = ctx.targets[0].pitch;
	u32 tpitchmat = ctx.targets[1].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	u32 ty = ctx.offset.y;

	u8 draw_mask = draw_on_background ? (u8)Material::Background : (u8)Material::Dirt;
	u8 draw_mat = draw_on_background ? (u8)Material::Dirt : (u8)Material::Background;

	for(; hleft-- > 0; tp += tpitch, tpmat += tpitchmat, fp += fpitch, ++ty) {
		u32 tx = ctx.offset.x;

		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			Material mat = Material(tpmat[i]);

			u8 mask = c.a();

			if (mask && (mat.flags & draw_mask)) {
				auto org = tl::Color::read(tp + i*4);

				u32 mx = (tx + i) & 15, my = ty & 15;
				auto bg = tl::Color::read(&tframe[my * 16 * 4 + mx * 4]);

				if (mask == 1 || mask == 2) {
					// Keep original index slot
					bg = org.blend_half(bg).with_a(org.a());
					tpmat[i] = (Material::Dirt | Material::Background);
				} else {
					tpmat[i] = draw_mat;
				}

				tl::Color::write(tp + i*4, bg);
			}
		}
	}
}

void draw_sprite_on_level(tl::BasicBlitContext<2, 1> ctx) {

	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* tpmat = ctx.targets[1].pixels;
	u8* fp = ctx.sources[0].pixels;

	u32 tpitch = ctx.targets[0].pitch;
	u32 tpitchmat = ctx.targets[1].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	for(; hleft-- > 0; tp += tpitch, tpmat += tpitchmat, fp += fpitch) {
		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			Material mat = Material(tpmat[i]);
			
			if (c.a()) {
				auto org = tl::Color::read(tp + i*4);

				tl::Color::write(tp + i*4, c);
				if (mat.background())
					tpmat[i] = Material::Dirt;
			}
		}
	}
}

}
