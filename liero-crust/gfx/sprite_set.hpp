#ifndef GFX_SPRITE_SET_HPP
#define GFX_SPRITE_SET_HPP

#include "tl/std.h"
#include "tl/rectpack.hpp"
#include "tl/shared_ptr.hpp"
#include "texture.hpp"

namespace gfx {

struct TiledTexture {
	tl::RectPack packer;
	Texture tex;
	tl::Image image;
	tl::RectU dirty_rect;

	TiledTexture(u32 w, u32 h, bool linear)
		: packer(tl::PackedRect(0, 0, w, h))
		, tex(w, h, linear)
		, image(w, h, 4) {
	}

	tl::PackedRect alloc(tl::VectorU2 dim, bool allow_rotate = false) {
		return packer.try_fit(dim, allow_rotate);
	}

	tl::PackedRect alloc(tl::ImageSlice const& slice) {
		auto r = this->alloc(slice.dim);
		if (!r.empty()) {
			image.blit(slice, 0, r.x1, r.y1);
			dirty_rect |= r;
		}
		return r;
	}

	GLuint id() {
		if (!dirty_rect.empty()) {
			tex.upload_subimage(image.crop(dirty_rect), dirty_rect.ul());
			dirty_rect = tl::RectU();
		}
		return tex.id;
	}
};

struct SpriteAcc {
	tl::PackedRect rect;
	tl::RcBoxed<TiledTexture> tex;

	SpriteAcc() = default;
	TL_MOVABLE(SpriteAcc);
	TL_NON_COPYABLE(SpriteAcc);

	SpriteAcc(tl::PackedRect rect, tl::RcBoxed<TiledTexture>&& tex)
		: rect(rect), tex(move(tex)) {
	}
};

struct SpriteSetAcc {
	tl::Vec<tl::RcBoxed<TiledTexture>> textures;

	SpriteSetAcc() = default;
	TL_MOVABLE(SpriteSetAcc);

	void extend() {
		textures.push_back(tl::rc_boxed(TiledTexture(256, 256, false)));
	}

	SpriteAcc alloc(tl::ImageSlice const& slice) {
		if (textures.empty()) {
			extend();
		}

		{
			auto& tex = textures.back();
			auto r = tex->alloc(slice);
			if (!r.empty())
				return SpriteAcc(r, tex.clone());
		}

		extend();

		{
			auto& tex2 = textures.back();
			auto r = tex2->alloc(slice);
			return SpriteAcc(r, tex2.clone());
		}
	}
};

}

#endif // GFX_SPRITE_SET_HPP
