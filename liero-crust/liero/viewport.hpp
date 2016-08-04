#ifndef LIERO_VIEWPORT_HPP
#define LIERO_VIEWPORT_HPP

#include <tl/vector.hpp>
#include <tl/rect.hpp>
#include <tl/image.hpp>

namespace liero {

struct State;

struct DrawTarget {
	DrawTarget(tl::ImageSlice image, tl::Palette& pal) : image(image), pal(pal) {
	}

	tl::ImageSlice image;
	tl::Palette& pal;
};

struct Viewport {
	u32 worm_idx;
	tl::VectorI2 offset;
	tl::RectU screen_pos;
	u32 width, height;

	Viewport(tl::RectU screen_pos)
		: worm_idx(0)
		, screen_pos(screen_pos)
		, width(screen_pos.width())
		, height(screen_pos.height()) {
	}

	void draw(State& state, DrawTarget& target);
};

}

#endif // LIERO_VIEWPORT_HPP
