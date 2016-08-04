#include "level.hpp"

#include "mod.hpp"

namespace liero {

u8 mat_from_index[] = {0, 9, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 0, 0, 1, 1, 1, 4, 4, 4, 1, 1, 1, 4, 4, 4, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 4, 4, 4, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 24, 24, 24, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

Level::Level(tl::source& src, tl::Palette& pal) {

	this->graphics.alloc_uninit(504, 350, 1);
	this->materials.alloc_uninit(504, 350, 1);

	src.ensure(this->graphics.size());
	memcpy(this->graphics.data(), src.begin(), this->graphics.size());

	u8* mat = this->materials.data();
	u8* index_bitmap = this->graphics.data();
	for (usize i = 0; i < this->graphics.size(); ++i) {
		mat[i] = mat_from_index[index_bitmap[i]];
	}

	this->graphics = this->graphics.convert(4, &pal);
}

void draw_level_effect(tl::BasicBlitContext<2, 1, true> ctx, u8 const* tframe) {

	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* tpmat = ctx.targets[1].pixels;
	u8* fp = ctx.sources[0].pixels;

	u32 tpitch = ctx.targets[0].pitch;
	u32 tpitchmat = ctx.targets[1].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	u32 ty = ctx.offset.y;

	for(; hleft-- > 0; tp += tpitch, tpmat += tpitchmat, fp += fpitch, ++ty) {
		u32 tx = ctx.offset.x;

		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			Material mat = Material(tpmat[i]);

			switch (c.a()) {
			case 6:
				if (mat.any_dirt()) {
					u32 mx = (tx + i) & 15, my = ty & 15;

					auto bg = tl::Color::read(&tframe[my * 16 * 4 + mx * 4]);

					tl::Color::write(tp + i*4, bg);
					tpmat[i] = mat_from_index[bg.a()];
				}
				break;

			case 1:
				if (mat.any_dirt()) {
					// Keep original index slot
					auto org = tl::Color::read(tp + i*4);
					tl::Color::write(tp + i*4, c.with_a(org.a()));
				}
				break;
			}
		}
	}
}

}
