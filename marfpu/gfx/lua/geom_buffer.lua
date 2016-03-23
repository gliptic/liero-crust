local gfx = ...

ffi.cdef [[
	enum { gb_vertex_buffer_size = 4096 };

	typedef struct
	{
		float vertices[gb_vertex_buffer_size*2];
		float texcoords[gb_vertex_buffer_size*2];
		float colors[gb_vertex_buffer_size*4];

		float* cur_vert;
		float* cur_tex;
		float* cur_col;
		float col[4];
		int last_texture;

		float firstx, firsty;

		int modes, geom_mode;
		bool continuation;
	} geom_buffer;

	float am_sinf(float x);
	float am_cosf(float x);
]]

local gb_textured = 0
local gb_quad = 0
local gb_line = 1
local gb_tri_fan = 2
local gb_line_strip = 3
local gb_tri = 4
local gb_point = 5
local gb_image = 6
local gb_tri_strip = 7

local C = ffi.C
local gl = gfx.gl
local g = gfx.g

local gb_vertex_buffer_size = C.gb_vertex_buffer_size

local geom_buffer = ffi.typeof "geom_buffer"
local buf = geom_buffer()
local tobit = bit.tobit

buf.cur_vert = buf.vertices
buf.cur_tex = buf.texcoords
buf.cur_col = buf.colors
buf.last_texture = 0
buf.continuation = false

function gfx.geom_flush()
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
end

function gfx.geom_clear()
	gfx.geom_flush()
	buf.cur_vert = buf.vertices
	buf.cur_tex = buf.texcoords
	buf.cur_col = buf.colors
	buf.last_texture = 0
	buf.continuation = false
end

function gfx.geom_lines()
	if buf.geom_mode ~= gb_line then
		gfx.geom_clear()
		buf.geom_mode = gb_line
	end
end

function gfx.geom_images()
	if buf.geom_mode ~= gb_image then
		gfx.geom_clear()
		buf.geom_mode = gb_image
	end
end

function gfx.geom_tri()
	if buf.geom_mode ~= gb_tri then
		gfx.geom_clear()
		buf.geom_mode = gb_tri
	end
end

function gfx.geom_points()
	if buf.geom_mode ~= gb_point then
		gfx.geom_clear()
		buf.geom_mode = gb_point
	end
end

function gfx.geom_tri_fan()
	-- Always start a new
	gfx.geom_clear()
	buf.geom_mode = gb_tri_fan
end

function gfx.geom_tri_strip()
	-- Always start a new
	gfx.geom_clear()
	buf.geom_mode = gb_tri_strip
end


function gfx.geom_color(r, g, b, a)
	buf.col[0] = r
	buf.col[1] = g
	buf.col[2] = b
	buf.col[3] = a
end

local function check_vertices(num)
	if tonumber(buf.cur_vert - buf.vertices) + num > gb_vertex_buffer_size*2 then
	--if (buf.vertices + gb_vertex_buffer_size*2) - buf.cur_vert < num then
		gfx.geom_flush()
	end
end

local function image_tex_coords(img)
	local new_tex = img[1]
	if new_tex ~= buf.last_texture then
		gfx.geom_flush()
		buf.last_texture = new_tex
	end
	buf.cur_tex[0] = img[2] -- x1
	buf.cur_tex[1] = img[3] -- y1

	buf.cur_tex[2] = img[4] -- x2
	buf.cur_tex[3] = img[3] -- y1

	buf.cur_tex[4] = img[4] -- x2
	buf.cur_tex[5] = img[5] -- y2

	buf.cur_tex[6] = img[2] -- x1
	buf.cur_tex[7] = img[5] -- y2
	buf.cur_tex = buf.cur_tex + 8
end

local function vertex_color(num)
	for i=1,num do
		ffi.copy(buf.cur_col, buf.col, 16)
		buf.cur_col = buf.cur_col + 4
	end
end

local function vertex_color_one()
	ffi.copy(buf.cur_col, buf.col, 16)
	buf.cur_col = buf.cur_col + 4
end

function gfx.tri(x1, y1, x2, y2, x3, y3)
	check_vertices(6)
	vertex_color(3)

	buf.cur_vert[0] = x1
	buf.cur_vert[1] = y1
	buf.cur_vert[2] = x2
	buf.cur_vert[3] = y2
	buf.cur_vert[4] = x3
	buf.cur_vert[5] = y3
	buf.cur_vert = buf.cur_vert + 6
end

function gfx.line(x1, y1, x2, y2)
	check_vertices(4)
	vertex_color(2)

	buf.cur_vert[0] = x1
	buf.cur_vert[1] = y1
	buf.cur_vert[2] = x2
	buf.cur_vert[3] = y2
	buf.cur_vert = buf.cur_vert + 4
end

function gfx.vertex(x1, y1)
	check_vertices(2)
	vertex_color_one()

	buf.cur_vert[0] = x1
	buf.cur_vert[1] = y1
	buf.cur_vert = buf.cur_vert + 2
end

function gfx.circle(x, y, radius)
	gfx.geom_tri()

	local ang = 0
	local step = math.pi / radius

	local x1, y1 = x + g.am_cosf(ang) * radius, y + g.am_sinf(ang) * radius

	while ang < 2*math.pi do
		ang = ang + step
		local x2, y2 = x + g.am_cosf(ang) * radius, y + g.am_sinf(ang) * radius
		gfx.tri(x1, y1, x2, y2, x, y)
		x1, y1 = x2, y2
	end
end

function gfx.ring(x, y, r1, r2)
	gfx.geom_tri()

	local ang = 0
	local step = math.pi / r1

	local c, s = g.am_cosf(ang), g.am_sinf(ang)
	local x1, y1 = x + c * r2, y + s * r2
	local x2, y2 = x + c * r1, y + s * r1

	while ang < 2*math.pi do
		ang = ang + step
		c, s = g.am_cosf(ang), g.am_sinf(ang)
		local x3, y3 = x + c * r2, y + s * r2
		local x4, y4 = x + c * r1, y + s * r1
		
		gfx.tri(x1, y1, x3, y3, x2, y2)
		gfx.tri(x3, y3, x4, y4, x2, y2)
		x1, y1 = x3, y3
		x2, y2 = x4, y4
	end
end

function gfx.image(x, y, img)
	check_vertices(8)
	image_tex_coords(img)
	vertex_color(4)

	local w = img.w
	local h = img.h

	buf.cur_vert[0] = x
	buf.cur_vert[1] = y

	buf.cur_vert[2] = x + w
	buf.cur_vert[3] = y

	buf.cur_vert[4] = x + w
	buf.cur_vert[5] = y + h

	buf.cur_vert[6] = x
	buf.cur_vert[7] = y + h
	buf.cur_vert = buf.cur_vert + 8
end

function gfx.rot_image(x, y, ang, scale, img)
	check_vertices(8)
	image_tex_coords(img)
	vertex_color(4)

	local w = img.w
	local h = img.h
	local halfw = w*scale*0.5
	local halfh = h*scale*0.5

	--local ax, ay = math.cos(ang), math.sin(ang)
	local ax, ay = g.am_cosf(ang), g.am_sinf(ang)
	
	local x1 = halfw*ax
	local y1 = halfw*ay
	local x2 = halfh*ay
	local y2 = halfh*ax

	local xul = x2 - x1
	local xur = x1 + x2
	local ylr = y1 + y2
	local yur = y1 - y2

	buf.cur_vert[0] = x + xul
	buf.cur_vert[1] = y - ylr

	buf.cur_vert[2] = x + xur
	buf.cur_vert[3] = y + yur

	buf.cur_vert[4] = x - xul
	buf.cur_vert[5] = y + ylr

	buf.cur_vert[6] = x - xur
	buf.cur_vert[7] = y - yur
	buf.cur_vert = buf.cur_vert + 8
end

return gfx