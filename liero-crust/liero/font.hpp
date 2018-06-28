#ifndef LIERO_FONT_HPP
#define LIERO_FONT_HPP

#include "../gfx/sprite_set.hpp"
#include "../gfx/geom_buffer.hpp"
#include "tl/string.hpp"

struct Font {
	tl::Vec<gfx::SpriteAcc> chars;

	void alloc_char(gfx::SpriteSetAcc& spriteSet, tl::ImageSlice slice) {
		this->chars.push_back(spriteSet.alloc(slice));
	}

	float measure_width(tl::StringSlice text, float size) const {
		float x = 0.f;
		for (auto c : text) {
			if (c >= 2 && c < 250 + 2) {
				auto const& ch = this->chars[c - 2];

				x += ch.rect.width() * size;
			}
		}

		return x;
	}

	void draw_text(gfx::GeomBuffer& buf, tl::StringSlice text, tl::VectorF2 pos, float size) {

		for (auto c : text) {
			if (c >= 2 && c < 250 + 2) {
				auto& ch = this->chars[c - 2];

				buf.tex_rect(
					pos.x, pos.y,
					pos.x + ch.rect.width() * size, pos.y + ch.rect.height() * size,
					ch.rect.x1 / 256.f, ch.rect.y1 / 256.f,
					ch.rect.x2 / 256.f, ch.rect.y2 / 256.f,
					ch.tex->id());

				pos.x += ch.rect.width() * size;
			}
		}
	}
};

#endif // LIERO_FONT_HPP