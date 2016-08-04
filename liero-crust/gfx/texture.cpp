#include "texture.hpp"

namespace gfx {

Texture::Texture(u32 width, u32 height) {
	GLint unpack_alignment;
	
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
	glGenTextures(1, &this->id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, 4,
			width, height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, 0);
#if 0
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
}

Texture::~Texture() {
	glDeleteTextures(1, &this->id);
}

}
