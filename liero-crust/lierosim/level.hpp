#ifndef LIERO_LEVEL_HPP
#define LIERO_LEVEL_HPP 1

#include <tl/cstdint.h>
#include <tl/vector.hpp>
#include <tl/image.hpp>
#include <tl/stream.hpp>

namespace liero {

struct Material {

	enum {
		Dirt = 1<<0,
		Dirt2 = 1<<1,
		Rock = 1<<2,
		Background = 1<<3,
		SeeShadow = 1<<4,
		WormM = 1<<5
	};

	u8 flags;

	Material(u8 flags)
		: flags(flags) {
	}

	bool dirt() const { return (flags & Dirt) != 0; }
	bool dirt2() const { return (flags & Dirt2) != 0; }
	bool rock() const { return (flags & Rock) != 0; }
	bool background() const { return (flags & Background) != 0; }
	bool see_shadow() const { return (flags & SeeShadow) != 0; }
		
	// Constructed
	bool dirt_rock() const { return (flags & (Dirt | Dirt2 | Rock)) != 0; }
	bool any_dirt() const { return (flags & (Dirt | Dirt2)) != 0; }
	bool dirt_back() const { return (flags & (Dirt | Dirt2 | Background)) != 0; }
	bool worm() const { return (flags & WormM) != 0; }
};

extern u8 mat_from_index[256];

struct Level {

	tl::Image materials;
	tl::Image graphics;

	Level(tl::source& src, tl::Palette& pal);

	~Level() = default;
	TL_DEFAULT_CTORS(Level);

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

void draw_level_effect(tl::BasicBlitContext<2, 1, true> ctx, u8 const* tframe);

}

#endif // LIERO_LEVEL_HPP
