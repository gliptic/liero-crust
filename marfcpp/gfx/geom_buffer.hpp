#ifndef GEOM_BUFFER_HPP
#define GEOM_BUFFER_HPP

#include "tl/vec.hpp"

namespace gfx {


struct geom_buffer {
	enum {
		gb_none,
		gb_point
	};

	static int const vertex_buffer_size = 4096;

	geom_buffer()
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

	tl::fvec2 first;
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

	void points() {
		if (this->geom_mode != gb_point) {
			this->clear();
			this->geom_mode = gb_point;
		}
	}

	void color(float r, float g, float b, float a) {
		this->col[0] = r;
		this->col[1] = g;
		this->col[2] = b;
		this->col[3] = a;
	}

	void vertex(tl::fvec2 v) {
		check_vertices(2);
		vertex_color(1);

		*this->cur_vert++ = v.x;
		*this->cur_vert++ = v.y;
	}

private:
	void check_vertices(std::size_t num) {
		if ((this->cur_vert - this->vertices) + num > vertex_buffer_size * 2) {
			flush();
		}
	}

	void vertex_color(std::size_t num) {
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
