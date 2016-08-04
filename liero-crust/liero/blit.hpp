#ifndef LIERO_BLIT_HPP
#define LIERO_BLIT_HPP 1

#include <tl/image.hpp>
#include <tl/string.hpp>

namespace liero {

void worm_blit(tl::BlitContext ctx);
void sobj_blit(tl::BlitContext ctx);
void draw_text_small(tl::ImageSlice dest, tl::ImageSlice font, tl::StringSlice text, i32 x, i32 y);

}

#endif // LIERO_BLIT_HPP
