local vec = ...

local gfx = import "libs/gfx"

ffi.cdef [[
typedef struct
{
	double x, y;
} vec;

typedef struct
{
	vec vert[3];
} triangle;

typedef struct triangulator triangulator;

triangulator* triangulator_create(double const* vertices, int* vertex_count, int ring_count);
triangle* triangulator_get_triangles(triangulator* self, int* count);
void triangulator_free(triangulator*);
]]

function vec.step_bezier(bez,segments)
	local lines = {}
	for t=0,segments do
		local x, y = eval_bezier(bez,t/segments)
		lines[#lines + 1] = {x, y}
	end

	qvi.polyline(lines)
end

local function eval_bezier(bez,t)
	local nt = 1 - t
	local nt2 = nt*nt
	local nt3 = nt*nt*nt
	local t2 = t*t
	local t3 = t*t*t

	local x = nt3*bez[1][1] + 3*t*nt2*bez[2][1] + 3*t2*nt*bez[3][1] + t3*bez[4][1]
	local y = nt3*bez[1][2] + 3*t*nt2*bez[2][2] + 3*t2*nt*bez[3][2] + t3*bez[4][2]

	return x, y
end

local function vecrotate(v, rot)
	return {v[1]*rot[1] + v[2]*rot[2], v[1]*rot[2] - v[2]*rot[1]}
end

local function vecnormalize(v)
	local mag = math.sqrt(v[1]*v[1] + v[2]*v[2])
	if mag == 0 then
		return {0, 0}
	end
	return {v[1] / mag, v[2] / mag}
end

local function vecadd(a,b)
	return {a[1]+b[1],a[2]+b[2]}
end

local function vecsub(a,b)
	return {a[1]-b[1],a[2]-b[2]}
end

local function vecmul(s,b)
	return {s*b[1],s*b[2]}
end

local function transform_rs(bez)
	local r = vecnormalize(vecsub(bez[2], bez[1]))
	
	return {
		{0,0},
		vecrotate(vecsub(bez[2], bez[1]), r),
		vecrotate(vecsub(bez[3], bez[1]), r),
		vecrotate(vecsub(bez[4], bez[1]), r)
	}
end

local function transform_rs_single_s(bez, v)
	local r = vecnormalize(vecsub(bez[2], bez[1]))
	
	return (v[1] - bez[1][1])*r[2] - (v[2] - bez[1][2])*r[1]
end

local function split_bezier(bez,t)
	local p0p = vecadd(bez[1], vecmul(t, vecsub(bez[2], bez[1])))
	local p1p = vecadd(bez[2], vecmul(t, vecsub(bez[3], bez[2])))

	local p2p = vecadd(bez[3], vecmul(t, vecsub(bez[4], bez[3])))
	local p0pp = vecadd(p0p, vecmul(t, vecsub(p1p, p0p)))
	local p1pp = vecadd(p1p, vecmul(t, vecsub(p2p, p1p)))
	local p0ppp = vecadd(p0pp, vecmul(t, vecsub(p1pp, p0pp)))

	return {bez[1],p0p,p0pp,p0ppp}, {p0ppp,p1pp,p2p,bez[4]}
end

local function split_bezier_second(bez,t)
	local p0p = vecadd(bez[1], vecmul(t, vecsub(bez[2], bez[1])))
	local p1p = vecadd(bez[2], vecmul(t, vecsub(bez[3], bez[2])))

	local p2p = vecadd(bez[3], vecmul(t, vecsub(bez[4], bez[3])))
	local p0pp = vecadd(p0p, vecmul(t, vecsub(p1p, p0p)))
	local p1pp = vecadd(p1p, vecmul(t, vecsub(p2p, p1p)))
	local p0ppp = vecadd(p0pp, vecmul(t, vecsub(p1pp, p0pp)))

	return {p0ppp,p1pp,p2p,bez[4]}
end

local function flatten_inflection(bez, t, f)
	-- Clip at unreasonable values
	if t > 1e30 then
		return 1, 1
	elseif t < -1e30 then
		return 0, 0
	end

	local second = split_bezier_second(bez, t)
	--local sub = transform_rs(second)
	local s = transform_rs_single_s(second, second[4])
	
	--local tf = math.pow((f / math.abs(sub[4][2])), (1/3))
	local tf = math.pow((f / math.abs(s)), (1/3))
	
	if tf ~= tf then
		print(f, s)
	end

	local offset = math.abs(tf*(1 - t))
	local m, p = t - offset, t + offset
	
	--[[
	if m < 0 then m = 0
	elseif m > 1 then m = 1 end

	if p < 0 then p = 0
	elseif p > 1 then p = 1 end
	]]

	return m, p
end

local function circular_flatten_once(bez, t, f)
	assert(t >= 0 and t <= 1)
	local second = split_bezier_second(bez, t)
	--local sub = transform_rs(second)
	local s = transform_rs_single_s(second, second[3])

	local tp = math.sqrt(f / (3 * math.abs(s)))

	return t + tp*(1 - t)
end

function vec.acc_bezier(bez, f, vectors, add_start)
	local x0,y0 = bez[1][1],bez[1][2]
	local x1,y1 = bez[2][1],bez[2][2]
	local x2,y2 = bez[3][1],bez[3][2]
	local x3,y3 = bez[4][1],bez[4][2]

	local ax = -x0 + 3*x1 - 3*x2 + x3
	local ay = -y0 + 3*y1 - 3*y2 + y3
	local bx = 3*x0 - 6*x1 + 3*x2
	local by = 3*y0 - 6*y1 + 3*y2
	local cx = -3*x0 + 3*x1
	local cy = -3*y0 + 3*y1
	local dx = x0
	local dy = y0

	local t1m, t1p, t2m, t2p = 0, 0, 1, 1

	local tcusp = ((ay*cx - ax*cy)/(ax*by - ay*bx))/2
	local sqr = tcusp*tcusp - ((by*cx - bx*cy)/(ay*bx - ax*by))/3
	if sqr >= 0 then
		local o = math.sqrt(sqr)

		local t1 = tcusp - o
		local t2 = tcusp + o

		t1m, t1p = flatten_inflection(bez, t1, f)
		t2m, t2p = flatten_inflection(bez, t2, f)
	end

	if t1p > t2m and tcusp >= 0 and tcusp <= 1 then
		-- Clip at tcusp
		t1p = tcusp
		t2m = tcusp
	end

	local x, y = nil, nil -- = eval_bezier(bez, 0)

	--print("Segments: " .. t1m .. ", " .. t1p .. " - " .. t2m .. ", " .. t2p)

	local function add_point(nx, ny, t, s)
		--print("Time " .. t .. ", " .. s)
		if x ~= nx or y ~= ny then
			--print(nx .. ", " .. ny .. " - " .. x .. )
			vectors:pushback(nx)
			vectors:pushback(ny)
			--gfx.line(x, y, nx, ny)
			--gfx.vertex(nx, ny)
			--print("Time " .. t .. " (" .. nx .. ", " .. ny .. ")")
			--print(vectors:size())
			x, y = nx, ny
		else
			print("Omitting point at (" .. nx .. ", " .. ny .. ")")
		end
	end

	if add_start then
		add_point(eval_bezier(bez, 0), 0, "start")
	end

	local function circular_flatten(from, to)
		--print(type .. ", from " .. from .. ", to " .. to)
		while true do
			from = circular_flatten_once(bez, from, f)
			
			if from >= to then
				local nx, ny = eval_bezier(bez, to)
				add_point(nx, ny, to, "circular end")
				break
			end

			local nx, ny = eval_bezier(bez, from)
			add_point(nx, ny, from, "circular")
		end
	end

	local pos = 0

	-- First
	if t1m > pos then
		local max = math.min(t1m, 1)
		circular_flatten(0, max)
		pos = max
	end

	-- Second
	if t1p > pos and t1p < 1 then
		local nx, ny = eval_bezier(bez, t1p)
		add_point(nx, ny, t1p, "2")
		pos = t1p
	end

	-- Third
	if t2m > pos and pos < 1 then
		local max = math.min(t2m, 1)
		circular_flatten(pos, max)
		pos = max
	end

	-- Fourth
	if t2p > pos and t2p < 1 then
		local nx, ny = eval_bezier(bez, t2p)
		add_point(nx, ny, t2p, "4")
		pos = t2p
	end

	-- TODO: Flatten from the end for better symmetry (maybe?)

	-- Fifth
	if pos < 1 then
		circular_flatten(pos, 1)
	end
end

return vec