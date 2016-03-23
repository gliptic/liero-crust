local gfx = ...

ffi.cdef [[

	typedef struct
	{
	   void           *userdata;
	   unsigned char  *data;         // pointer to .ttf file
	   int             fontstart;    // offset of start of font

	   int numGlyphs;                // number of glyphs, needed for range checking

	   int loca,head,glyf,hhea,hmtx; // table locations as offset from start of .ttf
	   int index_map;                // a cmap mapping for our chosen character encoding
	   int indexToLocFormat;         // format needed to map from glyph index to glyph
	} tl_tt_fontinfo;

	float tl_tt_ScaleForPixelHeight(const tl_tt_fontinfo *info, float pixels);
	int tl_tt_InitFont(tl_tt_fontinfo *info, const unsigned char *data, int offset);
	int tl_tt_FindGlyphIndex(const tl_tt_fontinfo *info, int unicode_codepoint);
	void tl_tt_GetCodepointHMetrics(const tl_tt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
	tl_image tl_tt_GetCodepointBitmap(const tl_tt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *xoff, int *yoff);
]]

local tl_image = ffi.typeof "tl_image"
local tl_tt_fontinfo = ffi.typeof "tl_tt_fontinfo"
local g = gfx.g
local C = ffi.C

local function create_image(w, h, bpp)
	local img = tl_image()
	img.w = w
	img.h = h
	img.bpp = bpp
	img.pitch = w * bpp
	img.pixels = C.malloc(w * h * bpp)
	return img
end

local font_mt = {
	char = function(font, ch, size)
		if font.cache[ch] then
			return font.cache[ch]
		end

		local scale = g.tl_tt_ScaleForPixelHeight(font.struct, 80)

		local params = ffi.new("int[4]")
		local img = g.tl_tt_GetCodepointBitmap(font.struct, scale, scale, ch, params, params + 1)

		g.tl_tt_GetCodepointHMetrics(font.struct, ch, params + 2, params + 3);

		local char_data = {
			gfx.acc_image(img),
			params[0],
			params[1],
			math.floor(params[2] * scale),
			params[3] * scale
		}
		font.cache[ch] = char_data
		return char_data
	end,

	draw = function(font, x, y, text)
		for i=1,#text do
			local c = font:char(string.byte(text, i))

			gfx.image(x + c[2], y + c[3], c[1])
			x = x + c[4]
		end
	end
}

font_mt.__index = font_mt

function gfx.load_font(path)
	local f = io.open(path, "rb")
	local data = f:read("*a")
	f:close()

	print("Loading font: " .. #data .. " bytes")
	local struct = tl_tt_fontinfo()
	local r = g.tl_tt_InitFont(struct, data, 0)

	return setmetatable({__data = data, struct = struct, cache = {}}, font_mt)
end



return gfx