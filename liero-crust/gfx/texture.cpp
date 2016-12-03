#include "texture.hpp"

namespace gfx {

Texture::Texture(u32 width, u32 height, bool linear) {
	GLint unpack_alignment;
	
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
	glGenTextures(1, &this->id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, 4,
			width, height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, 0);

	GLenum filtering = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
}

void Texture::upload_subimage(tl::ImageSlice const& slice, u32 x, u32 y) {
	this->bind();

	glPixelStorei(GL_UNPACK_ROW_LENGTH, slice.pitch / slice.bpp);

	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, slice.dim.x, slice.dim.y,
		slice.bpp == 4 ? GL_RGBA : GL_RED,
		GL_UNSIGNED_BYTE, slice.pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

Texture::~Texture() {
	glDeleteTextures(1, &this->id);
}

}
