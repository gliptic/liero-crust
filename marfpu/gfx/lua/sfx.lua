local sfx = {}

local gfx = import "libs/gfx"

ffi.cdef [[
	typedef struct
	{
		char dummy;
	} sfx;

	typedef struct
	{
		unsigned sample_rate;
		int outputIsRunning;
		unsigned int successfulCalls;
		int outputUnderflowCount;
		uint32_t stream_pos;
		int16_t* buffer;
		void* ud;
		void (*fill)(struct sfx_stream* str, uint32_t start, uint32_t frames);
		sfx* owner;
	} sfx_stream;

	typedef struct sfx_sound 
	{
		tl_vector samples;
	} sfx_sound;

	sfx* sfx_create(void);
	sfx_stream* sfx_open(sfx* self, void (*fill)(sfx_stream* str, uint32_t start, uint32_t frames));
	int sfx_start(sfx_stream* str);
	int sfx_stop(sfx_stream* str);
	void sfx_stream_destroy(sfx_stream* str);

	typedef struct sfx_mixer sfx_mixer;
	typedef struct sfx_sound sfx_sound;

	sfx_mixer* sfx_mixer_create(void);
	void       sfx_set_volume(sfx_mixer* self, uint32_t h, double speed);
	uint32_t   sfx_mixer_add(sfx_mixer* self, sfx_sound* snd, uint32_t time);
	void       sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames);
	sfx_sound* sfx_load_wave(tl_byte_source_pullable* src);
	uint32_t   sfx_stream_get_pos(sfx_stream* self);

	int tl_vorbis_decode(tl_byte_source_pullable bs, short** output, int* channels);
]]

local g = gfx.g
local s = g

local context
local stream
local mixer
local sound

function sfx.open()
	if stream then
		return
	end
	mixer = s.sfx_mixer_create()
	context = assert(s.sfx_create())
	stream = assert(s.sfx_open(context, s.sfx_mixer_fill))
	stream.ud = mixer
	assert(s.sfx_start(stream) == 0)

	local bs = ffi.new("tl_byte_source_pullable")
	g.tl_bs_file_source(bs, "E:/test.ogg")

	local output = ffi.new "short*[1]"
	local channels = ffi.new "int[1]"
	local len = g.tl_vorbis_decode(bs, output, channels)

	sound = ffi.new "sfx_sound"
	sound.samples.cap = len
	sound.samples.size = len
	sound.samples.impl = output[0]

	--[[
	local bs = ffi.new("tl_byte_source_pullable")
	g.tl_bs_file_source(bs, "f:/test.wav")
	sound = assert(s.sfx_load_wave(bs))
	]]
end

function sfx.close()
	if stream then
		g.sfx_stop(stream)
		g.sfx_stream_destroy(stream)
		stream = nil
	end
end

function sfx.play()
	s.sfx_mixer_add(mixer, sound, s.sfx_stream_get_pos(stream))
end

function sfx.pos() return stream.stream_pos end
function sfx.calls() return stream.successfulCalls end
function sfx.underflows() return stream.outputUnderflowCount end

return sfx