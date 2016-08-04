#ifndef GEOM_BUFFER_HPP
#define GEOM_BUFFER_HPP

#include "tl/vector.hpp"
#include <tl/std.h>

namespace gfx {


struct GeomBuffer {
	enum {
		gb_none,
		gb_point,
		gb_quad
	};

	static int const vertex_buffer_size = 4096;

	GeomBuffer()
	: cur_vert(vertices)
	, cur_tex(texcoords)
	, cur_col(colors)
	, last_texture(0)
	, geom_mode(gb_none)
	, continuation(false) {
		col[0] = 0;
		col[1] = 0;
		col[2] = 0;
		col[3] = 0;
	}

	float vertices[vertex_buffer_size * 2],
		texcoords[vertex_buffer_size * 2],
		colors[vertex_buffer_size * 4];

	float* cur_vert;
	float* cur_tex;
	float* cur_col;
	float col[4];
	int last_texture;

	tl::VectorF2 first;
	int modes, geom_mode;
	bool continuation;

	void flush();

	void clear() {
		this->flush();
		this->cur_vert = vertices;
		this->cur_tex = texcoords;
		this->cur_col = colors;

		this->last_texture = 0;
		this->continuation = false;
	}

	void ensure_mode(int mode) {
		if (this->geom_mode != mode) {
			this->clear();
			this->geom_mode = mode;
		}
	}

	void points() {
		ensure_mode(gb_point);
	}

	void quads() {
		ensure_mode(gb_quad);
	}

	void color(float r, float g, float b, float a) {
		this->col[0] = r;
		this->col[1] = g;
		this->col[2] = b;
		this->col[3] = a;
	}

	void unsafe_vertex(float x, float y) {
		*this->cur_vert++ = x;
		*this->cur_vert++ = y;
	}

	void unsafe_texcoord(float x, float y) {
		*this->cur_tex++ = x;
		*this->cur_tex++ = y;
	}

	void vertex(tl::VectorF2 v) {
		check_vertices(2);
		vertex_color(1);

		*this->cur_vert++ = v.x;
		*this->cur_vert++ = v.y;
	}

	void tex_rect(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2, int texture) {
		assert(this->geom_mode == gb_quad);

		if (this->last_texture != texture) {
			this->clear();
			this->last_texture = texture;
		}

		check_vertices(4);
		vertex_color(4);
		unsafe_vertex(x1, y1);
		unsafe_vertex(x2, y1);
		unsafe_vertex(x2, y2);
		unsafe_vertex(x1, y2);

		unsafe_texcoord(tx1, ty1);
		unsafe_texcoord(tx2, ty1);
		unsafe_texcoord(tx2, ty2);
		unsafe_texcoord(tx1, ty2);
	}
	
private:
	void check_vertices(usize num) {
		if ((this->cur_vert - this->vertices) + num > vertex_buffer_size * 2) {
			flush();
		}
	}

	void vertex_color(usize num) {
		for (int i = 0; i < num; ++i) {
			*this->cur_col++ = col[0];
			*this->cur_col++ = col[1];
			*this->cur_col++ = col[2];
			*this->cur_col++ = col[3];
		}
	}
};

}

#endif // GEOM_BUFFER_HPP
