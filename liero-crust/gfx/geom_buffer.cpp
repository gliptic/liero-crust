#include "geom_buffer.hpp"
#include "GL\glew.h"

namespace gfx {

void GeomBuffer::flush() {
	if (this->cur_vert == vertices) {
		return;
	}

	if (!this->continuation) {
		this->first = tl::VectorF2(vertices[0], vertices[1]);
	}

	GLsizei vertex_count = GLsizei(this->cur_vert - this->vertices) / 2;

	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	glColor3d(1, 1, 1);

	if (this->last_texture != 0) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, this->last_texture);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, this->texcoords);
	} else {
		glDisable(GL_TEXTURE_2D);
	}

	glVertexPointer(2, GL_FLOAT, 0, this->vertices);
	glColorPointer(4, GL_FLOAT, 0, this->colors);

	if (this->geom_mode == gb_point) {
		glDrawArrays(GL_POINTS, 0, vertex_count);
	} else if (this->geom_mode == gb_quad) {
		glDrawArrays(GL_QUADS, 0, vertex_count);
	}

	if (this->last_texture != 0) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	this->last_texture = 0;
	this->continuation = false;
	this->cur_vert = vertices;
	this->cur_tex = texcoords;
	this->cur_col = colors;

	/*
	if buf.cur_vert == buf.vertices then
		return
	end

	if not buf.continuation then
		buf.firstx = buf.vertices[0]
		buf.firsty = buf.vertices[1]
	end

	local vertex_count = math.floor((buf.cur_vert - buf.vertices) / 2)

	gl.glEnableClientState(gl.GL_COLOR_ARRAY)
	gl.glEnableClientState(gl.GL_VERTEX_ARRAY)

	gl.glColor3d(1, 1, 1)

	if buf.last_texture ~= 0 then
		gl.glEnable(gl.GL_TEXTURE_2D)
		gl.glBindTexture(gl.GL_TEXTURE_2D, buf.last_texture)
		gl.glEnableClientState(gl.GL_TEXTURE_COORD_ARRAY)
		gl.glTexCoordPointer(2, gl.GL_FLOAT, 0, buf.texcoords)
	else
		gl.glDisable(gl.GL_TEXTURE_2D)
	end

	gl.glVertexPointer(2, gl.GL_FLOAT, 0, buf.vertices)
	gl.glColorPointer(4, gl.GL_FLOAT, 0, buf.colors)

	if buf.geom_mode == gb_line then
		gl.glDrawArrays(gl.GL_LINES, 0, vertex_count)
	elseif buf.geom_mode == gb_tri then
		gl.glDrawArrays(gl.GL_TRIANGLES, 0, vertex_count)
	elseif buf.geom_mode == gb_tri_fan then
		gl.glDrawArrays(gl.GL_TRIANGLE_FAN, 0, vertex_count)
	elseif buf.geom_mode == gb_tri_strip then
		gl.glDrawArrays(gl.GL_TRIANGLE_STRIP, 0, vertex_count)
	elseif buf.geom_mode == gb_point then
		gl.glDrawArrays(gl.GL_POINTS, 0, vertex_count)
	else
		gl.glDrawArrays(gl.GL_QUADS, 0, vertex_count)
	end

	if buf.last_texture ~= 0 then
		gl.glDisableClientState(gl.GL_TEXTURE_COORD_ARRAY)
	end

	gl.glDisableClientState(gl.GL_VERTEX_ARRAY)
	gl.glDisableClientState(gl.GL_COLOR_ARRAY)

	local prevy = buf.cur_vert[-1]
	local prevx = buf.cur_vert[-2]
	local prevy2 = buf.cur_vert[-3]
	local prevx2 = buf.cur_vert[-4]

	buf.cur_vert = buf.vertices
	buf.cur_tex = buf.texcoords
	buf.cur_col = buf.colors

	if buf.geom_mode == gb_tri_fan then
		gfx.vertex(buf.firstx, buf.firsty)
		gfx.vertex(prevx, prevy)
		buf.continuation = true
	elseif buf.geom_mode == gb_tri_strip then
		gfx.vertex(prevx2, prevy2)
		gfx.vertex(prevx, prevy)
		buf.continuation = true
	else
		buf.last_texture = 0
		buf.continuation = false
	end
	*/
}

}