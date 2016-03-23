local qvi = ...

local gfx = import "libs/gfx"

local mode_set = false

local function ensure_window()
	if not mode_set then
		mode_set = true
		gfx.set_mode(1024, 768, false)
	end
end

function qvi.init()
	ensure_window()
end

function qvi.polyline(points)
	ensure_window()
	gfx.geom_lines()
	local fx = points[1][1]
	local fy = points[1][2]
	for i=2,#points do
		local x, y = points[i][1], points[i][2]
		gfx.line(fx, fy, x, y)
		fx, fy = x, y
	end
end

--[[
function qvi.circle(x,y,r)
	ensure_window()
	gfx.circle(x,y,r)
end

function qvi.geom_color(r,g,b,a)
	ensure_window()
	gfx.geom_color(r,g,b,a)
end]]

local prev_time = gfx.timer()
local first_display = true

function qvi.display(fps)
	ensure_window()

	if not first_display then
		gfx.geom_flush()
		gfx.end_drawing()

		local delay = 1/fps

		repeat
			gfx.sleep(2)
			local now = gfx.timer()
			if prev_time + delay < now then
				prev_time = now
				break
			end
		until false

		gfx.clear()
	else
		first_display = false
	end

	return not gfx.update()
end

return qvi