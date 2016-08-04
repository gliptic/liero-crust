#ifndef GFX_HPP
#define GFX_HPP 1

#include "GL/glew.h"
#include <tl/std.h>
#include <tl/image.hpp>

namespace gfx {

struct Texture {
	Texture()
		: id(0) {
	}

	Texture(Texture&& other)
		: id(other.id) {
		other.id = 0;
	}

	Texture(u32 width, u32 height);
	~Texture();

	Texture(Texture const&) = delete;
	Texture& operator=(Texture const&) = delete;

	void bind() {
		glBindTexture(GL_TEXTURE_2D, this->id);
	}

	void upload_subimage(tl::ImageSlice const& slice, u32 x = 0, u32 y = 0) {
		this->bind();

		glPixelStorei(GL_UNPACK_ROW_LENGTH, slice.pitch / slice.bpp);

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, slice.dim.x, slice.dim.y, GL_RGBA, GL_UNSIGNED_BYTE, slice.pixels);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}

	GLuint id;
};

}

#endif // GFX_HPP