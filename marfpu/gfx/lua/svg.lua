local svg = ...

local xml = import "libs/xml"

local commands = {
	M = {2},
	C = {6},
	S = {4},
	Q = {4},
	T = {2},
	Z = {0},
	L = {2},
	H = {1},
	V = {1}
}

local function parse_color(str)
	if not str then return end

	if string.byte(str, 1) == 35 then
		local hex = string.sub(str, 2)
		return tonumber(hex, 16)
	end

	-- Otherwise don't know
end

local function handle_body(body, paths)
	--local paths = {}

	for _,v in ipairs(body) do
		if type(v) == "table" then
			--print("Reading path")
			
			if v.label == "g" then
				handle_body(v, paths)
			elseif v.label == "path" then
				local fill = parse_color(v.xarg.fill)

				local def = v.xarg.d

				local orgx, orgy
				local x, y = 0, 0 -- Current point
				local px2, py2 = x, y
				local path = {}

				local params = {0,0,0,0,0,0}
				local paramloc = 1
				local curcmd, currel = nil, nil

				local curnum, hasdot = "", false

				local finish_command, finish_num
				local subpaths = {}

				function finish_command()
					if curcmd then
						finish_num()

						local expectedparams = commands[curcmd][1]

						if expectedparams > paramloc - 1 then
							print("Expected " .. expectedparams .. " params for " .. curcmd .. 
								" got " .. (paramloc-1))
						end

						if currel then
							if curcmd == 'V' then
								params[1] = params[1] + y
							else
								for i=1,6,2 do
									params[i] = params[i] + x
									params[i+1] = params[i+1] + y
								end
							end
						end

						if curcmd == 'M' then
							orgx = params[1]
							orgy = params[2]
							x = orgx
							y = orgy
						elseif curcmd == 'C' then
							path[#path + 1] = {
								{x, y},
								{params[1], params[2]},
								{params[3], params[4]},
								{params[5], params[6]}
							}
							x, y = params[5], params[6]
							px2, py2 = params[3], params[4]
						elseif curcmd == 'S' then
							local x1, y1 = 2*x - px2, 2*y - py2
							path[#path + 1] = {
								{x, y},
								{x1, y1},
								{params[1], params[2]},
								{params[3], params[4]}
							}
							x, y = params[3], params[4]
							px2, py2 = params[1], params[2]
						elseif curcmd == 'Z' then
							-- TODO: Mark the curve as closed. DON'T draw a line back to origin
							--paths[#paths + 1] = path
							subpaths[#subpaths + 1] = path
							--print("Added subpath, total = " .. #subpaths)
							path = {}
						elseif curcmd == 'L' then
							path[#path + 1] = {
								{x, y},
								{params[1], params[2]},
								{x, y},
								{params[1], params[2]}
							}

							x, y = params[1], params[2]
						elseif curcmd == 'H' then
							path[#path + 1] = {
								{x, y},
								{params[1], y},
								{x, y},
								{params[1], y}
							}

							x = params[1]
						elseif curcmd == 'V' then
							path[#path + 1] = {
								{x, y},
								{x, params[1]},
								{x, y},
								{x, params[1]}
							}

							y = params[1]
						else
							print("Unknown command ".. curcmd)
						end

						if curcmd ~= 'C' and curcmd ~= 'S' then
							px2, py2 = x, y
						end

						--print("Command " .. curcmd .. " " .. x .. ", " .. y)
						paramloc = 1
						-- Reset defaults
						for i=1,6 do
							params[i] = 0
						end
					end
				end

				function finish_num()
					if curnum ~= "" then
						--print("Number: " .. curnum)
						local num = tonumber(curnum)
						-- Clear curnum to avoid infinite recursion
						curnum = ""
						--print("Parsed number: " .. num)
						if paramloc-1 == commands[curcmd][1] then
							finish_command()
							if curcmd == 'M' then
								curcmd = 'L'
							end
						end

						params[paramloc] = num
						paramloc = paramloc + 1

						curnum = ""
						hasdot = false
					end
				end

				for i=1,#def do
					local c = string.byte(def, i)
					
					local rel = (c >= 97)
					if rel and c < 123 then
						c = c - 32
					end

					local ch = string.char(c)
					local cmd = commands[ch]

					if cmd then
						finish_command()
						curcmd = ch
						currel = rel
					elseif (c >= 48 and c <= 57) or c == 46 then -- digit or dot
						if c == 46 then
							if hasdot then
								finish_num()
							end
							hasdot = true
						end
						curnum = curnum .. ch
					elseif c == 44 or c == 32 or c == 10 or c == 13 or c == 9 then -- comma-wsp
						finish_num()
					elseif c == 45 then -- minus
						finish_num()
						curnum = "-"
					else
						print("Unknown character " .. ch)
					end
				end

				finish_command()
				subpaths.fill = fill or 0
				if subpaths[1] then
					--print("Read " .. #subpaths .. " subpaths")
					paths[#paths + 1] = subpaths
				end
			end
		end
	end

	return paths
end

function svg.load(path)
	local f = io.open(path, "rb")
	local data = f:read("*a")
	f:close()

	local tree = xml.collect(data)

	for _,v in ipairs(tree) do
		if type(v) == "table" then
			if v.label == "svg" then
				local paths = {}
				handle_body(v, paths)
				return paths
			end
		end
	end
end

return svg