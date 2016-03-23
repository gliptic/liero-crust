local tl = ...

local gfx = import "libs/gfx"

function tl.def_vector_type(t)

	local tp = t .. "*"
	local size = ffi.sizeof(t)
	local self = "cast('tl_vector*', self_)"
	
	local mt = assert(loadstring([[
		local mt = {}
		mt.__index = mt

		local C = ffi.C
		local cast = ffi.cast

		local function enlarge(self, extra)
			local newcap = self.size * 2 + extra
			local s = newcap*]] .. size .. [[
			self.impl = C.realloc(self.impl, s)
			self.cap = newcap
		end

		function mt.init(self_)
			local self = ]]..self..[[

			self.size = 0
			self.cap = 0
			self.impl = nil
		end
		
		function mt.pushback(self_, e)
			local self = ]]..self..[[
			local s = self.size

			if s >= self.cap then
				enlarge(self, 1)
			end

			ffi.cast(']] .. tp .. [[', self.impl)[s] = e
			self.size = s + 1
		end

		function mt.get(self_, i)
			return ffi.cast(']] .. tp .. [[', ]]..self..[[.impl)[i]
		end

		function mt.ptr(self_)
			return ffi.cast(']] .. tp .. [[', ]]..self..[[.impl)
		end

		function mt.size(self_)
			return (]]..self..[[).size
		end

		function mt.__gc(self_)
			C.free(]]..self..[[.impl)
		end

		return mt
	]]))()

	return function()
		local s = ffi.sizeof("tl_vector")
		local u = create_udata(s)
		debug.setmetatable(u, mt)
		u:init()
		return u
	end
end

return tl