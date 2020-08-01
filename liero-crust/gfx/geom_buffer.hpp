#ifndef GEOM_BUFFER_HPP
#define GEOM_BUFFER_HPP

#include "tl/vector.hpp"
#include "texture.hpp"
#include <tl/std.h>
#include <tl/vector.hpp>

namespace gfx {


struct GeomBuffer {
	enum {
		gb_none,
		gb_point,
		gb_quad,
		gb_tri
	};

	static int const vertex_buffer_size = 4096;

	GeomBuffer(Texture& white_texture)
	: cur_vert(vertices)
	, cur_tex(texcoords)
	, cur_col(colors)
	, last_texture(0)
	, white_texture_id(white_texture.id)
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
	int white_texture_id;

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

	void tris() {
		ensure_mode(gb_tri);
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

	void unsafe_vertex(tl::VectorF2 v) {
		*this->cur_vert++ = v.x;
		*this->cur_vert++ = v.y;
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

	void set_texture(int texture) {
		if (this->last_texture != texture) {
			this->clear();
			this->last_texture = texture;
		}
	}

	void tex_rect(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2, int texture) {
		assert(this->geom_mode == gb_quad);

		set_texture(texture);

		check_vertices(8);
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

	void rect(float x1, float y1, float x2, float y2) {
		assert(this->geom_mode == gb_quad);

		check_vertices(8);
		vertex_color(4);
		unsafe_vertex(x1, y1);
		unsafe_vertex(x2, y1);
		unsafe_vertex(x2, y2);
		unsafe_vertex(x1, y2);
	}

	void tri(float x1, float y1, float x2, float y2, float x3, float y3) {
		assert(this->geom_mode == gb_tri);

		check_vertices(6);
		vertex_color(3);
		unsafe_vertex(x1, y1);
		unsafe_vertex(x2, y2);
		unsafe_vertex(x3, y3);
	}

	void tri_rect(tl::VectorF2 ul, tl::VectorF2 lr) {
		assert(this->geom_mode == gb_tri);

		check_vertices(12);
		vertex_color(6);
		unsafe_vertex(ul);
		unsafe_vertex(lr.x, ul.y);
		unsafe_vertex(ul.x, lr.y);
		unsafe_vertex(lr.x, ul.y);
		unsafe_vertex(lr);
		unsafe_vertex(ul.x, lr.y);
	}

	void tri_quad(tl::VectorF2 a, tl::VectorF2 b, tl::VectorF2 c, tl::VectorF2 d) {
		assert(this->geom_mode == gb_tri);

		check_vertices(12);
		vertex_color(6);
		unsafe_vertex(a);
		unsafe_vertex(b);
		unsafe_vertex(d);
		unsafe_vertex(b);
		unsafe_vertex(c);
		unsafe_vertex(d);
	}

	// TODO: This shouldn't be exposed
	void vertex_color(usize num) {
		for (int i = 0; i < num; ++i) {
			*this->cur_col++ = col[0];
			*this->cur_col++ = col[1];
			*this->cur_col++ = col[2];
			*this->cur_col++ = col[3];
		}
	}
	
private:
	void check_vertices(usize num) {
		if ((this->cur_vert - this->vertices) + num > vertex_buffer_size * 2) {
			flush();
		}
	}

	
};

}

#endif // GEOM_BUFFER_HPP
