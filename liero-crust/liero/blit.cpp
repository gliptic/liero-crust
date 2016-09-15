#include "blit.hpp"
#include "../lierosim/level.hpp"
#include <tl/rand.hpp>

namespace liero {

#define DO_LINE(body_) { \
	i32 cx = from.x; \
	i32 cy = from.y; \
	i32 dx = to.x - from.x; \
	i32 dy = to.y - from.y; \
	i32 sx = sign(dx); \
	i32 sy = sign(dy); \
	dx = std::abs(dx); \
	dy = std::abs(dy); \
	if (dx > dy) { \
		i32 c = -(dx >> 1); \
		while (cx != to.x) { \
			c += dy; \
			cx += sx; \
			if (c > 0) { \
				cy += sy; \
				c -= dx; } \
			body_ } \
	} else { \
		i32 c = -(dy >> 1); \
		while (cy != to.y) { \
			c += dx; \
			cy += sy; \
			if (c > 0) { \
				cx += sx; \
				c -= dy; } \
			body_ } } }

void draw_ninjarope(tl::ImageSlice img, tl::VectorI2 from, tl::VectorI2 to, tl::VecSlice<tl::Color> colors) {

	u32 max_color = u32(colors.size());
	u32 color = 0;

	DO_LINE({
		if (++color >= max_color) {
			color = 0;
		}

		if ((u32)cx < img.width()
		 && (u32)cy < img.height()) {
			img.unsafe_pixel32(u32(cx), u32(cy)) = colors[color].v;
		}
	});
}

static tl::Color mk_worm_color(u32 scale, u32 add, tl::Color c) {
	return tl::Color(
		(add * 4 + c.r() * scale) / 64,
		(add * 4 + c.g() * scale) / 64,
		(add * 4 + c.b() * scale) / 64,
		c.a());
}

void worm_blit(tl::BlitContext ctx, tl::Color worm_color) {
	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;
	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	tl::Color colors[] = {
		mk_worm_color(38, 0, worm_color),
		mk_worm_color(50, 0, worm_color),
		mk_worm_color(64, 0, worm_color),
		mk_worm_color(47, 1008, worm_color),
		mk_worm_color(28, 2205, worm_color)
	};

	for(; hleft-- > 0; tp += tpitch, fp += fpitch) {
		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			if (c.a()) {
				u8 idx = c.a();
				if (idx >= 30 && idx <= 34) {
#if 0
					i32 k = (61 * c.r() - 36 * c.b()) / 25;
					i32 m = c.r() - k;

					c = tl::Color(
						k + worm_color.r() * m / 144,
						k + worm_color.g() * m / 144,
						k + worm_color.b() * m / 144,
						c.a());
#else
					c = colors[idx - 30];
#endif
				}

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

void muzzle_fire_blit(tl::BlitContext ctx, tl::Palette& pal, int level) {
	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;
	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	u8 thresh, sub;

	switch (level) {
		case 0: thresh = 117; sub = 5; break;
		case 1: thresh = 115; sub = 3; break;
		case 2: thresh = 113; sub = 1; break;
		default: thresh = 1; sub = 0; break;
	}

	for(; hleft-- > 0; tp += tpitch, fp += fpitch) {
		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			auto tc = tl::Color::read(tp + i*4);

			if (c.a() >= thresh) {
				tl::Color::write(tp + i*4, pal.entries[c.a() - sub]);
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
