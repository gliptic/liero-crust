#ifndef GFX_HPP
#define GFX_HPP 1

#include "GL/glew.h"
#include <tl/std.h>
#include <tl/gfx/image.hpp>

namespace gfx {

struct Texture {
	Texture()
		: id(0) {
	}

	Texture(Texture&& other)
		: id(other.id) {
		other.id = 0;
	}

	Texture& operator=(Texture&& other) {
		this->~Texture();
		this->id = other.id;
		other.id = 0;
		return *this;
	}

	Texture(u32 width, u32 height, bool linear = false);
	~Texture();

	Texture(Texture const&) = delete;
	Texture& operator=(Texture const&) = delete;

	void bind() {
		glBindTexture(GL_TEXTURE_2D, this->id);
	}

	void upload_subimage(tl::ImageSlice const& slice, tl::VectorU2 x = tl::VectorU2());

	GLuint id;
};

}

#endif // GFX_HPP
