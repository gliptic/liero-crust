#include "level.hpp"

#include "mod.hpp"
#include "state.hpp"

namespace liero {

Level::Level(tl::Source& src, tl::Palette& pal, ModRef& mod) {

	this->graphics.alloc_uninit(504, 350, 1);
	alloc_mat(504, 350);

	src.ensure(this->graphics.size());
	memcpy(this->graphics.data(), src.begin(), this->graphics.size());

	u8* index_bitmap = this->graphics.data();

	for (usize y = 0; y < 350; ++y) {
		for (usize x = 0; x < 504; ++x) {
			usize i = y*504 + x;
			u8 m = mod.materials[index_bitmap[i]];

			usize o = y * this->materials_pitch + x / 8;

			this->materials_bitmap_dirt[o] = this->materials_bitmap_dirt[o] | (Material(m).any_dirt() << (x % 8));
			this->materials_bitmap_back[o] = this->materials_bitmap_back[o] | (Material(m).background() << (x % 8));
			this->materials_bitmap_dirt_rock[o] = this->materials_bitmap_dirt_rock[o] | (Material(m).dirt_rock() << (x % 8));
		}
	}

	this->graphics = this->graphics.convert(4, &pal);

}

bool Level::validate_mat() {
	return true;
}

void draw_level_effect(tl::VectorI2 pos, Level& level, tl::VecSlice<LargeSpriteRow> mframe, bool draw_on_background);

void draw_level_effect(State& state, tl::VectorI2 pos, i16 level_effect, bool graphics, TransientState& transient_state) {

	if (level_effect >= 0) {

		LevelEffectRecord le;
		le.pos = pos;
		le.level_effect = level_effect;
		
		transient_state.level_effect_queue.push_back(le);
	}
}

void TransientState::apply_queues(State& state) {
	for (auto& le : this->level_effect_queue) {
	
		auto pos = le.pos;
		auto level_effect = le.level_effect;

		auto& effect = state.mod.get_level_effect(u32(level_effect));

		auto mframe = state.mod.large_sprites.crop_square_sprite_v(effect.mframe());
		if (graphics) {
			u32 tframe_idx = effect.sframe();
			auto graphics_slice = state.level.graphics.slice().crop_bottom(1);
			auto tframe = state.mod.large_sprites.crop_square_sprite_v(tframe_idx);

			tl::ImageSlice targets[1] = { graphics_slice };
			tl::ImageSlice sources[1] = { mframe };

			tl::BasicBlitContext<1, 1, true> ctx(targets, sources, pos.x - 7, pos.y - 7);

			draw_level_effect(ctx, state.level.mat_iter(), tframe.pixels, effect.draw_back());
		} else {

			tl::VecSlice<LargeSpriteRow> mframe_slice(
				state.mod.large_sprites_bits.begin() + effect.mframe() * 16,
				state.mod.large_sprites_bits.begin() + effect.mframe() * 16 + 16);

			draw_level_effect(pos - tl::VectorI2(7, 7), state.level, mframe_slice, effect.draw_back());
		}
	}

#if !UPDATE_POS_IMMEDIATE
	for (u32 i = 0; i < state.nobjects.count; ++i) {
		auto new_pos = this->next_nobj_pos[i].pos;
		auto& self = state.nobjects.of_index(i);
		self.pos = new_pos;
		if (self.cell < 0) {
			auto ipos = new_pos.cast<i32>();
			self.cell = state.nobject_broadphase.update(narrow<CellNode::Index>(state.nobjects.index_of(&self)), ipos, self.cell);
		}
	}
#endif
}

void draw_level_effect(tl::VectorI2 pos, Level& level, tl::VecSlice<LargeSpriteRow> mframe, bool draw_on_background) {
	if (pos.y <= -16 || pos.y >= (i32)(level.graphics.height() - 1)
	 || pos.x <= -16 || pos.x >= (i32)level.graphics.width())
		return;

	if (pos.y < 0) { mframe.unsafe_cut_front(-pos.y); pos.y = 0; }
	else if (pos.y > level.graphics.height() - 16 - 1) { mframe.unsafe_cut_back(pos.y - (level.graphics.height() - 16 - 1)); }

	auto mat_iter = level.mat_iter();
	mat_iter.skip_rows((usize)pos.y);

	if (!draw_on_background) {
		for (auto row : mframe) {
			int xoffs = (pos.x & 7);
			u32 to_dirt = mat_iter.read_dirt_16(pos.x);
			u32 to_back = mat_iter.read_back_16(pos.x);
			u32 to_dirt_rock = mat_iter.read_dirt_rock_16(pos.x);
			u32 bit0 = u32(row.bit0) << xoffs;
			u32 bit1 = u32(row.bit1) << xoffs;

			u32 dirt_write = to_dirt & ~bit1;
			u32 back_write = to_back | (to_dirt & bit0);
			u32 dirt_rock_write = to_dirt_rock ^ (to_dirt & bit1);

			mat_iter.write_dirt_16(pos.x, dirt_write);
			mat_iter.write_back_16(pos.x, back_write);
			mat_iter.write_dirt_rock_16(pos.x, dirt_rock_write);

			mat_iter.next_row();
		}
	} else {
		for (auto row : mframe) {
			int xoffs = (pos.x & 7);
			u32 to_dirt = mat_iter.read_dirt_16(pos.x);
			u32 to_back = mat_iter.read_back_16(pos.x);
			u32 to_dirt_rock = mat_iter.read_dirt_rock_16(pos.x);
			u32 bit0 = u32(row.bit0) << xoffs;
			u32 bit1 = u32(row.bit1) << xoffs;

			u32 dirt_write = to_dirt | (to_back & bit0);
			u32 back_write = to_back & ~bit1;
			u32 dirt_rock_write = to_dirt_rock | dirt_write;

			mat_iter.write_dirt_16(pos.x, dirt_write);
			mat_iter.write_back_16(pos.x, back_write);
			mat_iter.write_dirt_rock_16(pos.x, dirt_rock_write);

			mat_iter.next_row();
		}
	}
}

void draw_level_effect(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter, u8 const* tframe, bool draw_on_background) {

	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;

	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	u32 ty = ctx.offset.y;
	mat_iter.skip_rows(ty);

	u8 draw_mat = draw_on_background ? (u8)Material::Dirt : (u8)Material::Background;

	if (draw_on_background) {
		for(; hleft-- > 0; tp += tpitch, fp += fpitch, ++ty) {
			u32 tx = ctx.offset.x;

			for (u32 i = 0; i < w; ++i) {
				auto c = tl::Color::read(fp + i*4);
				bool back = mat_iter.read_back(tx);

				u8 mask = c.a();

				if (mask && back) {
					auto org = tl::Color::read(tp + i*4);

					u32 mx = tx & 15, my = ty & 15;
					auto bg = tl::Color::read(&tframe[my * 16 * 4 + mx * 4]);

					if (mask <= 2) {
						// Keep original index slot
						bg = org.blend_half(bg).with_a(org.a());
						mat_iter.write(tx, Material::Dirt | Material::Background);
					} else {
						mat_iter.write(tx, draw_mat);
					}

					tl::Color::write(tp + i*4, bg);
				}

				++tx;
			}

			mat_iter.next_row();
		}
	} else {
		for(; hleft-- > 0; tp += tpitch, fp += fpitch, ++ty) {
			u32 tx = ctx.offset.x;

			for (u32 i = 0; i < w; ++i) {
				auto c = tl::Color::read(fp + i*4);
				bool dirt = mat_iter.read_dirt(tx);

				u8 mask = c.a();

				if (mask && dirt) {
					auto org = tl::Color::read(tp + i*4);

					u32 mx = tx & 15, my = ty & 15;
					auto bg = tl::Color::read(&tframe[my * 16 * 4 + mx * 4]);

					if (mask <= 2) {
						// Keep original index slot
						bg = org.blend_half(bg).with_a(org.a());
						mat_iter.write(tx, Material::Dirt | Material::Background);
					} else {
						mat_iter.write(tx, draw_mat);
					}

					tl::Color::write(tp + i*4, bg);
				}

				++tx;
			}

			mat_iter.next_row();
		}
	}
}

void draw_sprite_on_level(tl::BasicBlitContext<1, 1, true> ctx, MaterialIterator mat_iter) {

	u32 hleft = ctx.dim.y;
	u32 w = ctx.dim.x;
	u8* tp = ctx.targets[0].pixels;
	u8* fp = ctx.sources[0].pixels;

	u32 tpitch = ctx.targets[0].pitch;
	u32 fpitch = ctx.sources[0].pitch;

	u32 ty = ctx.offset.y;
	mat_iter.skip_rows(ty);

	for(; hleft-- > 0; tp += tpitch, fp += fpitch) {
		u32 tx = ctx.offset.x;

		for (u32 i = 0; i < w; ++i) {
			auto c = tl::Color::read(fp + i*4);
			bool back = mat_iter.read_back(tx);
			
			if (c.a()) {
				auto org = tl::Color::read(tp + i*4);

				tl::Color::write(tp + i*4, c);
				if (back) {
					mat_iter.write(tx, Material::Dirt);
				}
			}
			++tx;
		}

		mat_iter.next_row();
	}
}

}
