#ifndef LIERO_LEVEL_HPP
#define LIERO_LEVEL_HPP 1

#include <liero-sim/config.hpp>
#include <liero-sim/material.hpp>
#include <tl/cstdint.h>
#include <tl/vector.hpp>
#include <tl/gfx/image.hpp>
#include <tl/io/stream.hpp>

namespace liero {

struct State;
struct ModRef;

struct Level {

	tl::Image materials;
	tl::Image graphics;

	Level(tl::Source& src, tl::Palette& pal, ModRef& mod);

	~Level() = default;
	TL_DEFAULT_CTORS(Level);

	void copy_from(Level const& other, bool copy_graphics = false) {
		materials.copy_from(other.materials);
		if (copy_graphics) {
			graphics.copy_from(other.graphics);
		} else {
			graphics.alloc_uninit(other.graphics.width(), other.graphics.height(), other.graphics.bytespp());
		}
	}

	Material unsafe_mat(tl::VectorI2 pos) const {
		return Material(materials.unsafe_pixel8((u32)pos.x, (u32)pos.y));
	}

	Material mat_wrap(tl::VectorI2 pos) const {
		u32 idx = (u32)pos.x + (u32)pos.y * materials.slice().pitch;

		if (idx < materials.size()) {
			return materials.data()[idx];
		}

		return Material(0);
	}

	bool is_inside(tl::VectorI2 pos) const {
		return this->materials.is_inside(pos);
	}
};

void draw_level_effect(State& state, tl::VectorI2 pos, i16 level_effect);
void draw_level_effect(tl::BasicBlitContext<2, 1, true> ctx, u8 const* tframe, bool draw_on_background);
void draw_sprite_on_level(tl::BasicBlitContext<2, 1> ctx);

}

#endif // LIERO_LEVEL_HPP
