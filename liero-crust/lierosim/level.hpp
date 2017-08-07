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
struct TransientState;
struct ModRef;

struct MaterialIterator {
	u8* bitmap;
	usize bitmap_pitch;
	usize bitmap_size;

	MaterialIterator(u8* bitmap
		, usize bitmap_pitch
		, usize bitmap_size
	)
		: bitmap(bitmap)
		, bitmap_pitch(bitmap_pitch)
		, bitmap_size(bitmap_size)
	{
	}

	u8* dirt_p() { return bitmap; }
	u8* back_p() { return bitmap + bitmap_size; }
	u8* dirt_rock_p() { return bitmap + 2 * bitmap_size; }

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

	u32 read_back_word32(i32 word_x) {
		return *((u32 const *)back_p() + word_x);
	}

	u32 read_dirt_word32(i32 word_x) {
		return *((u32 const *)dirt_p() + word_x);
	}

	u32 read_dirt_rock_word32(i32 word_x) {
		return *((u32 const *)dirt_rock_p() + word_x);
	}

	u32 read_rock_word32(i32 word_x) {
		return read_dirt_rock_word32(word_x) & ~read_dirt_word32(word_x);
	}

	u32 read_rock_16(i32 x) {
		return read_dirt_rock_16(x) & ~read_dirt_16(x);
	}

	void skip_rows(usize rows) {
		bitmap += bitmap_pitch * rows;
	}

	void next_row() {
		bitmap += bitmap_pitch;
	}

	MaterialIterator as_skipped_rows(usize rows) {
		return MaterialIterator(bitmap + bitmap_pitch * rows, bitmap_pitch, bitmap_size);
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
	usize materials_pitch, materials_pitch_words;
	usize materials_bitmap_size;
	tl::VectorU2 materials_dim;

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
		// TODO: If not interleaving materials, the left and right red-zone can share the same space
		if (materials_dim.x != w || materials_dim.y != h) {
			free(this->materials_bitmap);
			usize w_bytes = 4 + 4 * ((w + 31) / 32) + 4;
			usize bitmap_size = w_bytes * h;
			usize bitmap_total_size = bitmap_size * mat_channels;
			u8* buf_ptr = (u8 *)malloc(bitmap_total_size);
			memset(buf_ptr, 0, bitmap_total_size);
			this->materials_bitmap = buf_ptr;

			this->materials_bitmap_dirt = buf_ptr + 4; // Top-right
			this->materials_bitmap_back = materials_bitmap_dirt + bitmap_size;
			this->materials_bitmap_dirt_rock = materials_bitmap_dirt + 2 * bitmap_size;

			this->materials_pitch = w_bytes;
			this->materials_pitch_words = w_bytes / 4;
			this->materials_bitmap_size = bitmap_size;
			this->materials_dim = tl::VectorU2((u32)w, (u32)h);
		}
	}

	void copy_from(Level const& other, bool copy_graphics = false) {

		if (copy_graphics) {
			graphics.copy_from(other.graphics);
		} else {
			graphics.clear();
		}

		alloc_mat(other.graphics.width(), other.graphics.height());

		memcpy(materials_bitmap, other.materials_bitmap, this->materials_bitmap_size * mat_channels);
	}

	MaterialIterator mat_iter() {
		return MaterialIterator(
			materials_bitmap_dirt,
			materials_pitch,
			materials_bitmap_size
		);
	}

	static bool read_bit(u8 const* p, u32 n) {
		// TODO: Handle platforms without masking shift
		return (((u32 const*)p)[n >> 5] & (1 << n)) != 0;
	}

	bool mat_dirt_rock(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return true;

		return read_bit(this->materials_bitmap_dirt_rock + (u32)pos.y * materials_pitch, (u32)pos.x);
	}

	bool mat_dirt(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return false;

		return read_bit(this->materials_bitmap_dirt + (u32)pos.y * materials_pitch, (u32)pos.x);
	}

	bool mat_wrap_back(tl::VectorI2 pos) const {
		if (!this->is_inside(pos))
			return false;

		return read_bit(this->materials_bitmap_back + (u32)pos.y * materials_pitch, (u32)pos.x);
	}

	bool is_inside(tl::VectorI2 pos) const {
		return ((u32)pos.x < this->materials_dim.x) && ((u32)pos.y < this->materials_dim.y);
	}
};

void draw_level_effect(State& state, tl::VectorI2 pos, i16 level_effect, bool graphics, TransientState& transient_state);
void draw_level_effect(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter, u8 const* tframe, bool draw_on_background);

void draw_sprite_on_level(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter);

}

#endif // LIERO_LEVEL_HPP
