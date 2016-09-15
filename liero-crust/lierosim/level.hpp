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

struct MaterialIterator {
	u8* bitmap;
	usize bitmap_pitch;

	MaterialIterator(
		u8* bitmap,
		usize bitmap_pitch)
			: bitmap(bitmap), bitmap_pitch(bitmap_pitch) {
	}

	u8* dirt_p() { return bitmap; }
	u8* back_p() { return bitmap + bitmap_pitch; }
	u8* dirt_rock_p() { return bitmap + 2 * bitmap_pitch; }

	u32 read_dirt_16(i32 x) {
		return *(u32 const *)(dirt_p() + (x >> 3));
	}

	void write_dirt_16(i32 x, u32 bits) {
		*(u32 *)(dirt_p() + (x >> 3)) = bits;
	}

	u32 read_back_16(i32 x) {
		return *(u32 const *)(back_p() + (x >> 3));
	}

	void write_back_16(i32 x, u32 bits) {
		*(u32 *)(back_p() + (x >> 3)) = bits;
	}

	u32 read_dirt_rock_16(i32 x) {
		return *(u32 const *)(dirt_rock_p() + (x >> 3));
	}

	void write_dirt_rock_16(i32 x, u32 bits) {
		*(u32 *)(dirt_rock_p() + (x >> 3)) = bits;
	}

	void skip_rows(usize rows) {
		bitmap += bitmap_pitch * rows * 3;
	}

	void next_row() {
		bitmap += bitmap_pitch * 3;
	}

	bool read_dirt(usize x) {
		return (dirt_p()[x >> 3] >> (x & 7)) & 1;
	}

	bool read_back(usize x) {
		return (back_p()[x >> 3] >> (x & 7)) & 1;
	}

	void write(usize x, u8 m) {
		bool dirt = (m & 1);
		bool back = ((m >> 2) & 1);
		bool dirt_rock = (m | (m >> 1)) & 1;

		dirt_p()[x / 8] = (dirt_p()[x / 8] & ~(1 << (x % 8))) | (dirt << (x % 8));
		back_p()[x / 8] = (back_p()[x / 8] & ~(1 << (x % 8))) | (back << (x % 8));
		dirt_rock_p()[x / 8] = (dirt_rock_p()[x / 8] & ~(1 << (x % 8))) | (dirt_rock << (x % 8));
	}
};

struct Level {

	tl::Image graphics;

	u8* materials_bitmap;
	u8* materials_bitmap_dirt;
	u8* materials_bitmap_back;
	u8* materials_bitmap_dirt_rock;
	usize materials_pitch;
	usize materials_full_pitch;

	Level(tl::Source& src, tl::Palette& pal, ModRef& mod);

	~Level() = default;
	
	Level()
		: materials_bitmap(0)
	{
	}

	Level(Level&&) = default;
	Level(Level const&) = delete;
	Level& operator=(Level const&) = delete;
	Level& operator=(Level&&) = default;

	bool validate_mat();

	static usize const mat_channels = 3;

	void alloc_mat(usize w, usize h) {
		usize w_bytes = 4 + (w + 31) / 8;
		usize bitmap_size = w_bytes * h;
		usize bitmap_total_size = bitmap_size * mat_channels;
		u8* buf_ptr = (u8 *)malloc(bitmap_total_size);
		memset(buf_ptr, 0, bitmap_total_size);
		materials_bitmap = buf_ptr;
		materials_bitmap_dirt = buf_ptr + 4; // Top-right
		materials_bitmap_back = materials_bitmap_dirt + w_bytes;
		materials_bitmap_dirt_rock = materials_bitmap_dirt + 2 * w_bytes;
		materials_pitch = w_bytes;
		materials_full_pitch = w_bytes * mat_channels;

		assert(this->materials_full_pitch * this->graphics.height() == bitmap_total_size);
	}

	void copy_from(Level const& other, bool copy_graphics = false) {

		if (copy_graphics) {
			graphics.copy_from(other.graphics);
		} else {
			graphics.alloc_uninit(other.graphics.width(), other.graphics.height(), other.graphics.bytespp());
		}

		if (!this->materials_bitmap) {
			alloc_mat(graphics.width(), graphics.height());
		}
		memcpy(materials_bitmap, other.materials_bitmap, this->materials_full_pitch * this->graphics.height());
	}

	MaterialIterator mat_iter() {
		return MaterialIterator(
			materials_bitmap_dirt,
			materials_pitch
		);
	}

	bool mat_dirt_rock(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return true;

		return bool((this->materials_bitmap_dirt_rock[(u32)pos.y * materials_full_pitch + ((u32)pos.x >> 3)] >> (pos.x & 7)) & 1);
	}

	bool mat_dirt(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return true;

		return bool((this->materials_bitmap_dirt[(u32)pos.y * materials_full_pitch + ((u32)pos.x >> 3)] >> (pos.x & 7)) & 1);
	}

	bool mat_wrap_back(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return false;

		return bool((this->materials_bitmap_back[(u32)pos.y * materials_full_pitch + ((u32)pos.x >> 3)] >> (pos.x & 7)) & 1);
	}

	bool is_inside(tl::VectorI2 pos) const {
		return this->graphics.is_inside(pos);
	}
};

void draw_level_effect(State& state, tl::VectorI2 pos, i16 level_effect, bool graphics);
void draw_level_effect(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter, u8 const* tframe, bool draw_on_background);

void draw_sprite_on_level(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter);

}

#endif // LIERO_LEVEL_HPP
