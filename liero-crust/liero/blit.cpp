#include "blit.hpp"
#include "../lierosim/level.hpp"

namespace liero {

void worm_blit(tl::BlitContext ctx) {
	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;
	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	for(; hleft-- > 0; tp += tpitch, fp += fpitch) {
		// TODO: If c.a() is in worm range, translate to worm colours

		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			if (c.a()) {
				tl::Color::write(tp + i*4, c);
			}
		}
	}
}

void sobj_blit(tl::BlitContext ctx) {
	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;
	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	for(; hleft-- > 0; tp += tpitch, fp += fpitch) {
		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			auto tc = tl::Color::read(tp + i*4);

			if (c.a()
			 && (u32(tc.a()) - u32(160)) < u32(8)) {
				tl::Color::write(tp + i*4, c);
			}
		}
	}
}

void draw_text_small(tl::ImageSlice dest, tl::ImageSlice font, tl::StringSlice text, i32 x, i32 y) {
	x -= i32(text.size()) * 4 / 2;

	for (u8 c : text) {
		u32 idx = u32(c) - 'A';

		if (idx < 26) {
			dest.blit(font.crop(tl::RectU(0, idx * 4, 4, (idx + 1) * 4)), 0, x, y, tl::ImageSlice::BlitTransparent);
		}

		x += 4;
	}
}

}
