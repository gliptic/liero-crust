local lfs = require "lfs"

local function canonical_path(path)
	local pieces = {}
	for piece in string.gmatch(path, "[^\\/]") do
		if piece == '..' then
			if #pieces > 0 then
				pieces[#pieces] = nil
			end
		else
			pieces[#pieces + 1] = piece
		end
	end

	return table.concat(pieces, "/")
end

local function run(name, inputs, outputs, cmd)
	local oldest_output = math.huge
	for _,f in ipairs(outputs) do
		local m, err = lfs.attributes(f, "modification")
		if m then
			oldest_output = math.min(oldest_output, m)
		else
			oldest_output = 0
			break
		end
	end

	local newest_input = 0
	for _,f in ipairs(inputs) do
		local m, err = lfs.attributes(f, "modification")
		if not m then
			error(err)
		end
		newest_input = math.max(newest_input, m)
	end

	--print('Input time: ' .. newest_input .. ', output time: ' .. oldest_output)

	if newest_input >= oldest_output then
		print('Running ' .. name)
		cmd(inputs, outputs)
	end
end

return {
	run = run
}