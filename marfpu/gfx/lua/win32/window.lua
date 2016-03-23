local gfx = ...

ffi.cdef [[
	int QueryPerformanceCounter(uint64_t*);
	int QueryPerformanceFrequency(uint64_t*);

	void Sleep(unsigned);
	uint32_t timeGetTime(void);
]]

gfx.gl = ffi.load "OpenGL32"

local mm = ffi.load "winmm"

local box = ffi.new("uint64_t[1]")
assert(ffi.C.QueryPerformanceFrequency(box) ~= 0)

local invfreq = 1 / tonumber(box[0])

ffi.C.QueryPerformanceCounter(box)
local start = box[0]

function gfx.timer()
	ffi.C.QueryPerformanceCounter(box)
	return tonumber(box[0] - start) * invfreq
end

function gfx.sleep(ms)
	ffi.C.Sleep(ms)
end

local start = mm.timeGetTime()

function gfx.timer_ms()
	return bit.tobit(mm.timeGetTime() - start)
end

return gfx