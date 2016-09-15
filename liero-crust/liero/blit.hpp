#ifndef LIERO_BLIT_HPP
#define LIERO_BLIT_HPP 1

#include <tl/gfx/image.hpp>
#include <tl/string.hpp>

namespace liero {

void draw_ninjarope(tl::ImageSlice img, tl::VectorI2 from, tl::VectorI2 to, tl::VecSlice<tl::Color> colors);
void worm_blit(tl::BlitContext ctx, tl::Color worm_color);
void sobj_blit(tl::BlitContext ctx);
void draw_text_small(tl::ImageSlice dest, tl::ImageSlice font, tl::StringSlice text, i32 x, i32 y);
void muzzle_fire_blit(tl::BlitContext ctx, tl::Palette& pal, int level);

}

#endif // LIERO_BLIT_HPP
