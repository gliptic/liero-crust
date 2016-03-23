local gfx = {}; ffi.cdef [[
	enum
	{
		kbUnknown,
		kbEscape,
		kbRangeBegin = kbEscape,
		kbF1,
		kbF2,
		kbF3,
		kbF4,
		kbF5,
		kbF6,
		kbF7,
		kbF8,
		kbF9,
		kbF10,
		kbF11,
		kbF12,
		kb0,
		kb1,
		kb2,
		kb3,
		kb4,
		kb5,
		kb6,
		kb7,
		kb8,
		kb9,
		kbTab,
		kbReturn,
		kbSpace,
		kbLeftShift,
		kbRightShift,
		kbLeftControl,
		kbRightControl,
		kbLeftAlt,
		kbRightAlt,
		kbLeftMeta,
		kbRightMeta,
		kbBackspace,
		kbLeft,
		kbRight,
		kbUp,
		kbDown,
		kbHome,
		kbEnd,
		kbInsert,
		kbDelete,
		kbPageUp,
		kbPageDown,
		kbEnter,
		kbNumpad0,
		kbNumpad1,
		kbNumpad2,
		kbNumpad3,
		kbNumpad4,
		kbNumpad5,
		kbNumpad6,
		kbNumpad7,
		kbNumpad8,
		kbNumpad9,
		kbNumpadAdd,
		kbNumpadSubtract,
		kbNumpadMultiply,
		kbNumpadDivide,
		kbMinus,
		kbEquals,
	
		kbQ,
		kbW,
		kbE,
		kbR,
		kbT,
		kbY,
		kbU,
		kbI,
		kbO,
		kbP,
		kbLeftBracket,
		kbRightBracket,
		kbA,
		kbS,
		kbD,
		kbF,
		kbG,
		kbH,
		kbJ,
		kbK,
		kbL,
		kbSemicolon,
		kbApostrophe,
		kbGrave,    /* accent grave */
		kbBackslash,
		kbZ,
		kbX,
		kbC,
		kbV,
		kbB,
		kbN,
		kbM,
	
		kbRangeEnd = kbM,

		msRangeBegin,
		msLeft = msRangeBegin,
		msRight,
		msMiddle,
		msWheelUp,
		msWheelDown,
		msRangeEnd,
    
		gpRangeBegin,
		gpLeft = gpRangeBegin,
		gpRight,
		gpUp,
		gpDown,
		gpButton0,
		gpButton1,
		gpButton2,
		gpButton3,
		gpButton4,
		gpButton5,
		gpButton6,
		gpButton7,
		gpButton8,
		gpButton9,
		gpButton10,
		gpButton11,
		gpButton12,
		gpButton13,
		gpButton14,
		gpButton15,
		gpRangeEnd = gpButton15,
        
		kbNum = kbRangeEnd - kbRangeBegin + 1,
		msNum = msRangeEnd - msRangeBegin + 1,
		gpNum = gpRangeEnd - gpRangeBegin + 1,
    
		numButtons = gpRangeEnd,
		noButton = 0xffffffff
	};

	typedef struct
	{
		uint32_t cap;
		uint32_t size;
		void*    impl;
	} tl_vector;

	enum { ev_button = 0 };

	typedef struct
	{
		char ev;
		char down;
		union gfx_event_u
		{
			struct { int id; } button;
		} d;
	} gfx_event;

	typedef struct
	{
		int fsaa;
		unsigned int width;
		unsigned int height;
		char fullscreen;
		int mouseX, mouseY;
		char buttonState[numButtons + 1];
		tl_vector events;
	} gfx_common_window;

	gfx_common_window* gfx_window_create();
	int gfx_window_set_mode(gfx_common_window* self, uint32_t width, uint32_t height, int fullscreen);
	int gfx_window_set_visible(gfx_common_window* self, int state);
	int gfx_window_update(gfx_common_window* self);
	void gfx_window_end_drawing(gfx_common_window* self);
	int gfx_window_clear(gfx_common_window* self);
	void gfx_window_destroy(gfx_common_window* self);

	void glEnable(unsigned cap);
	void glDisable(unsigned cap);
	void glBegin(unsigned mode);
	void glEnd(void);
	void glVertex2d(double x, double y);
	void glColor3d(double r, double g, double b);
	void glTexCoord2d(double s, double t);

	void glGenTextures(int n, unsigned *textures);
	void glBindTexture(unsigned target, unsigned texture);
	void glTexImage2D(unsigned target, int level, int internalformat, int width, int height, int border, unsigned format, unsigned type, const void *pixels);
	void glTexParameteri(unsigned target, unsigned pname, int param);
	void glPixelStorei(unsigned pname, int param);
	void glTexSubImage2D(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, const void* data);

	void glEnableClientState(unsigned array);
	void glDisableClientState(unsigned array);
	void glVertexPointer(int size, unsigned type, int stride, const void* pointer);
	void glColorPointer(int size, unsigned type, int stride, const void* pointer);
	void glIndexPointer(unsigned type, int stride, const void* pointer);
	void glTexCoordPointer(int size, unsigned type, int stride, const void* pointer);
	void glDrawArrays(unsigned mode, int first, int count);

	void glClearColor(float red, float green, float blue, float alpha);
	void glScaled(double x, double y, double z);
	void glTranslated(double x, double y, double z);

	void glPointSize(float size);
	
	unsigned glGetError(void);

	enum {
		GL_TEXTURE_2D = 0x0DE1,
		GL_TEXTURE_MIN_FILTER = 0x2801,
		GL_TEXTURE_MAG_FILTER = 0x2800,
		GL_LINEAR = 0x2601,
		GL_COLOR_INDEX = 0x1900,
		GL_STENCIL_INDEX = 0x1901,
		GL_DEPTH_COMPONENT = 0x1902,
		GL_RED = 0x1903,
		GL_GREEN = 0x1904,
		GL_BLUE = 0x1905,
		GL_ALPHA = 0x1906,
		GL_RGB = 0x1907,
		GL_RGBA = 0x1908,
		GL_LUMINANCE = 0x1909,
		GL_LUMINANCE_ALPHA = 0x190A,
		GL_UNPACK_ALIGNMENT = 0x0CF5,

		GL_BYTE = 0x1400,
		GL_UNSIGNED_BYTE = 0x1401,
		GL_SHORT = 0x1402,
		GL_UNSIGNED_SHORT = 0x1403,
		GL_INT = 0x1404,
		GL_UNSIGNED_INT = 0x1405,
		GL_FLOAT = 0x1406,
		GL_2_BYTES = 0x1407,
		GL_3_BYTES = 0x1408,
		GL_4_BYTES = 0x1409,
		GL_DOUBLE = 0x140A,

		GL_VERTEX_ARRAY = 0x8074,
		GL_COLOR_ARRAY = 0x8076,
		GL_INDEX_ARRAY = 0x8077,
		GL_TEXTURE_COORD_ARRAY = 0x8078,

		GL_POINTS = 0x0000,
		GL_LINES = 0x0001,
		GL_LINE_LOOP = 0x0002,
		GL_LINE_STRIP = 0x0003,
		GL_TRIANGLES = 0x0004,
		GL_TRIANGLE_STRIP = 0x0005,
		GL_TRIANGLE_FAN = 0x0006,
		GL_QUADS = 0x0007,
		GL_QUAD_STRIP = 0x0008,
		GL_POLYGON = 0x0009
	};

	typedef struct tl_byte_source_pullable {
		uint8_t* buf;
		uint8_t* buf_end;
		void* ud;
		int (*pull)(struct tl_byte_source_pullable*);
		void (*free)(struct tl_byte_source_pullable*);
	} tl_byte_source_pullable;

	typedef struct tl_image {
		uint8_t* pixels;
		uint32_t w, h;
		uint32_t pitch;
		int bpp;
	} tl_image;

	int tl_image_convert(tl_image* to, tl_image* from);
	void tl_blit_unsafe(tl_image* to, tl_image* from, int x, int y);
	void tl_image_init(tl_image* self, uint32_t w, uint32_t h, int bpp);
	int tl_image_pad(tl_image* to, tl_image* from);

	typedef struct tl_png {
		tl_byte_source_pullable in;
		tl_image img;
	} tl_png;

	tl_png* tl_png_create(void);
	tl_png* tl_png_create_file(char const* path);
	int tl_png_load(tl_png* self, uint32_t req_comp);
	void tl_png_destroy(tl_png* self);

	int tl_bs_file_source(tl_byte_source_pullable* src, char const* path);

	typedef struct tl_recti {
		int x1, y1, x2, y2;
	} tl_recti;

	typedef struct tl_packed_rect
	{
		tl_recti rect;
	} tl_packed_rect;

	typedef struct tl_rectpack
	{
		tl_packed_rect base;
		struct tl_rectpack* parent;
		int largest_free_width, largest_free_height;
		tl_recti enclosing;
		struct tl_rectpack* ch[2];
		char occupied;
	} tl_rectpack;
	
	void            tl_rectpack_init(tl_rectpack* self, tl_recti r);
	tl_packed_rect* tl_rectpack_try_fit(tl_rectpack* self, int w, int h, int allow_rotate);
	void            tl_rectpack_remove(tl_rectpack* self, tl_packed_rect* r);
	void            tl_rectpack_deinit(tl_rectpack* self);

	typedef struct tl_list_node
	{
		struct tl_list_node* next;
		struct tl_list_node* prev;
	} tl_list_node;

	typedef struct
	{
		tl_list_node sentinel;
	} tl_list;

	void* realloc(void*, size_t);
	void free(void*);
]]

-- TODO: Move gfx_image to tl, make tl_png use it

if ffi.os == "Windows" then
	import("libs/win32/window", gfx)
end

local g = ffi.load("bonk")
gfx.g = g
local gl = gfx.gl
local tobit = bit.tobit

import("libs/geom_buffer", gfx)
import("libs/font", gfx)

local tl_recti = ffi.typeof "tl_recti"
local tl_rectpack = ffi.typeof "tl_rectpack"
local tl_image = ffi.typeof "tl_image"

function gfx.create_image(w, h, bpp)
	assert(bpp >= 1 and bpp <= 4)

	local img = tl_image()
	g.tl_image_init(img, w, h, bpp)
	return img
end

function gfx.load_image(path)
	local imgload = g.tl_png_create()
	g.tl_bs_file_source(imgload["in"], path)
	g.tl_png_load(imgload, 4)

	local ret = tl_image(imgload.img.pixels, imgload.img.w, imgload.img.h, imgload.img.pitch, imgload.img.bpp)
	imgload.img.pixels = nil
	g.tl_png_destroy(imgload)
	return ret
end

local function create_tex(w, h)
	gl.glPixelStorei(gl.GL_UNPACK_ALIGNMENT, 1)
	local tex = ffi.new("unsigned[1]")
	gl.glGenTextures(1, tex)
	print("GL:", tex[0], gl.glGetError())
	gl.glBindTexture(gl.GL_TEXTURE_2D, tex[0])
	gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, 4, w, h, 0, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE, nil)

	gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_LINEAR);
	gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_LINEAR);

	return tex[0]
end

local img_textures = {[1] = {}, [4] = {}}
local tex_size = 1024

local function create_image_texture()
	
	local packer = tl_rectpack()
	g.tl_rectpack_init(packer, tl_recti(0, 0, tex_size, tex_size))

	local tex = create_tex(tex_size, tex_size)

	return {packer, tex}
end

local function try_fit(w, h, pair, img)
	local r = g.tl_rectpack_try_fit(pair[1], w, h, false)

	if r ~= nil then
		local format
		if img.bpp == 4 then
			format = gl.GL_RGBA
		else
			format = gl.GL_ALPHA
		end

		local padded_img = gfx.create_image(w, h, img.bpp)
		g.tl_image_pad(padded_img, img)

		gl.glBindTexture(gl.GL_TEXTURE_2D, pair[2])
		gl.glTexSubImage2D(gl.GL_TEXTURE_2D,
			0,
			r.rect.x1, r.rect.y1,
			r.rect.x2 - r.rect.x1,
			r.rect.y2 - r.rect.y1,
			format,
			gl.GL_UNSIGNED_BYTE,
			padded_img.pixels);

		return {pair[2],
			(r.rect.x1 + 1) / tex_size,
			(r.rect.y1 + 1) / tex_size,
			(r.rect.x2 - 1) / tex_size,
			(r.rect.y2 - 1) / tex_size,
			w = img.w, h = img.h}
	end
	
	return nil
end

function gfx.acc_image(img)
	local w = img.w + 2 -- Padded
	local h = img.h + 2
	--local w = img.w
	--local h = img.h

	local bucket = img_textures[img.bpp]

	for i=1,#bucket do
		local pair = bucket[i]
		local r = try_fit(w, h, pair, img)
		if r ~= nil then
			return r
		end
	end

	-- TODO: This is an infinite loop with too large images. Adapt to image size.
	bucket[#bucket + 1] = create_image_texture()
	return gfx.acc_image(img)
end

local window = nil

function gfx.set_mode(w, h, fullscreen)
	if not window then
		window = g.gfx_window_create()
		gfx.window = window
	end

	window.fsaa = 4
	g.gfx_window_set_mode(window, w, h, fullscreen)
	g.gfx_window_set_visible(window, 1)

	gl.glPointSize(2)
end

function gfx.close()
	if window then
		g.gfx_window_destroy(window)
		window = nil
	end
end

function gfx.mouse_pos()
	return window.mouseX, window.mouseY
end

function gfx.on_button(id, down)
	-- Do nothing by default
end

function gfx.update()
	if g.gfx_window_update(window) == 0 then
		return false
	end

	for i=0,window.events.size-1 do
		local event = ffi.cast("gfx_event*", window.events.impl)[i]

		if event.ev == g.ev_button then
			gfx.on_button(event.d.button.id, event.down ~= 0)
		end
	end

	-- Clear event array
	window.events.size = 0

	return true
end

function gfx.clear()
	return g.gfx_window_clear(window)
end

function gfx.end_drawing()
	gfx.geom_flush()
	return g.gfx_window_end_drawing(window) ~= 0
end

function gfx.test()
	print(ffi.os)

	gfx.set_mode(1024, 768, false)

	--gl.glEnable(gl.GL_TEXTURE_2D)
	gl.glDisable(gl.GL_TEXTURE_2D)

	-- Load texture

	local accelerated_image = gfx.acc_image(gfx.load_image("E:/test.png"))
	--local font = gfx.load_font("c:/windows/fonts/arialbd.ttf")
	local font = gfx.load_font("c:/windows/fonts/impact.ttf")

	while gfx.update() do
		if window.buttonState[ffi.C.kbA] ~= 0 then
			break
		end

		gfx.clear()
		
		gfx.geom_images()
		gfx.image(20, 20, accelerated_image)

		--gfx.geom_color(1, 1, 1, 1)
		font:draw(20, 400, "Hello world")

		gfx.geom_color(1, 1, 1, 1)
		gfx.geom_lines()
		gfx.line(20, 20, 15, 15)

		gfx.line(20 + 789, 20 + 228, 25 + 789, 25 + 228)

		--789, 228

		gfx.end_drawing()
		C.Sleep(100)
	end
end

return gfx