local lfs = require "lfs"
local memo = require "memo"

local mod = {}

function map(tab, func)
	local newtab = {}
	for i=1,#tab do
		newtab[i] = func(tab[i])
	end

	return newtab
end

function mapi(tab, func)
	local newtab = {}
	for i=1,#tab do
		newtab[i] = func(tab[i], i-1)
	end

	return newtab
end

function foldl(tab, func)
	local result = nil
	for i=1,#tab do
		if result ~= nil then
			result = func(result, tab[i])
		else
			result = tab[i]
		end
	end

	return result
end

function getextlist(path, prefix)
	local extlist = {}
	local regex = "^" .. prefix
	for f in lfs.dir(path) do
		if f:match(regex) then
			extlist[#extlist + 1] = path .. "/" .. f
		end
	end
	return extlist
end

function parse_ext(filename)
	local functions, tokens, types, exacts = {}, {}, {}, {}
	local func_list = {}
	local extname, exturl, extstring

    local l = io.lines(filename)

	local extname = l()
	local exturl = l()
	local extstring = l()
	local ext = {}

	while true do
		local line = l()
		if not line then break end

		local line2 = line:match("^%s+(.*)")
		if line2 then
			repeat
				local exact = line2:match("^.*;$")
				if exact then
					exacts[#exacts + 1] = exact
					break
				end

				local typevalue, typename = line2:match("^typedef%s+(.+)%s+([%*A-Za-z0-9_]+)$")
				if typevalue then
					types[typename] = {name = typename, value = typevalue, ext = ext}
					break
				end

				local tokenname, tokenvalue = line2:match("^([A-Z][A-Z0-9_x]*)%s+([0-9A-Za-z_]+)$")

				if tokenname then
					tokens[tokenname] = {name = tokenname, value = tokenvalue, ext = ext}
					break
				end

				local funcreturn, funcname, funcparms =
					line2:match("^(.+) ([A-Za-z][A-Za-z0-9_]*) %((.+)%)$")

				if funcreturn then
					local func = {name = funcname, rtype = funcreturn, parms = funcparms, ext = ext}
					functions[funcname] = func
					func_list[#func_list + 1] = func
					break
				end

				error(line2 .. " matched no regexp")
			until true
		end
	end

	ext.name = extname
	ext.url = exturl
	ext.string = extstring
	ext.types = types
	ext.tokens = tokens
	ext.functions = functions
	ext.func_list = func_list
	ext.exacts = exacts

	return ext
end

function get_ext_set(path, prefix)
	local files = getextlist(path, prefix)
	local extlist = {}
	local func_name_to_func = {}
	local token_name_to_token = {}
	local ext_name_to_ext = {}
	local ext_to_id = {}
	local func_to_global_id = {}

	local func_id = 0

	for _,file in ipairs(files) do
		local ext = parse_ext(file)

		for _,func in ipairs(ext.func_list) do
			func_name_to_func[func.name] = func
			func_id = func_id + 1
			func_to_global_id[func] = func_id
		end

		for tname,token in pairs(ext.tokens) do
			token_name_to_token[tname] = token
		end

		ext_name_to_ext[ext.name] = ext
		extlist[#extlist + 1] = ext
		ext_to_id[ext] = #extlist
	end

	return {
		list = extlist,
		func_name_to_func = func_name_to_func, 
		token_name_to_token = token_name_to_token,
		ext_name_to_ext = ext_name_to_ext,
		ext_to_id = ext_to_id,
		func_to_global_id = func_to_global_id,
		func_count = func_id
	}
end

function merge_ext_sets(...)
	local new = {}

	local extlist = {}
	local func_name_to_func = {}
	local token_name_to_token = {}
	local ext_name_to_ext = {}
	local ext_to_id = {}
	local func_to_global_id = {}

	local func_id = 0

	for _,set in ipairs({...}) do
		for _,ext in ipairs(set.list) do
			extlist[#extlist + 1] = ext
			ext_to_id[ext] = #extlist
		end

		for tname,token in pairs(set.token_name_to_token) do
			token_name_to_token[tname] = token
		end

		for _,ext in ipairs(set.list) do
			ext_name_to_ext[ext.name] = ext

			for _,func in ipairs(ext.func_list) do
				func_name_to_func[func.name] = func
				func_id = func_id + 1
				func_to_global_id[func] = func_id
			end
		end
	end

	return {
		list = extlist,
		func_name_to_func = func_name_to_func, 
		token_name_to_token = token_name_to_token,
		ext_name_to_ext = ext_name_to_ext,
		ext_to_id = ext_to_id,
		func_to_global_id = func_to_global_id,
		func_count = func_id
	}
end

function ext_set_filter_used(set)
	local extlist = {}
	local func_name_to_func = {}
	local token_name_to_token = {}
	local ext_name_to_ext = {}
	local ext_to_id = {}
	local func_to_global_id = {}

	local func_id = 0

	for _,ext in ipairs(set.list) do
		if ext.used then
			print("Used extension: " .. ext.name, "str: " .. ext.string)
			extlist[#extlist + 1] = ext
			ext_to_id[ext] = #extlist
			ext_name_to_ext[ext.name] = ext

			for tname,token in pairs(ext.tokens) do
				token_name_to_token[tname] = token
			end

			for _,func in ipairs(ext.func_list) do
				if func.used then
					func_name_to_func[func.name] = func
					func_id = func_id + 1
					func_to_global_id[func] = func_id
				end
			end
		end
	end

	return {
		list = extlist,
		func_name_to_func = func_name_to_func, 
		token_name_to_token = token_name_to_token,
		ext_name_to_ext = ext_name_to_ext,
		ext_to_id = ext_to_id,
		func_to_global_id = func_to_global_id,
		func_count = func_id
	}
end

function printlines(out, tab)
	for _,l in ipairs(tab) do
		out:write(l)
		out:write("\n")
	end
end

function writeall(outpath, str)
	local f = io.open(outpath, "wb")
	if not f then error("Could not open: " .. outpath) end
	f:write(str)
	f:close()
end

function readall(inpath)
	local f = io.open(inpath, "rb")
	if not f then error("Not found: " .. inpath) end
	local str = f:read("*a")
	f:close()
	return str
end

function split(str, patt)
	local prev = 1
	local pieces = {}
	for before, after in str:gmatch("()" .. patt .. "()") do
		pieces[#pieces + 1] = str:sub(prev, before - 1)
		prev = after
	end

	pieces[#pieces + 1] = str:sub(prev, #str)
	return pieces
end

local Context = { }
Context.__index = Context

function expand_pattern(pattern, ctx)
	-- Replace $$ vars with contents of ctx

	local prev = 1
	local result = ""

	for before, v, after in pattern:gmatch("()%$([A-Z_]+)%$()") do
		result = result .. pattern:sub(prev, before - 1)
		--print(v)
		local value = ctx[v]
		if not value then
			for k,v in pairs(ctx) do
				print(k, v)
			end
			error("Unknown variable " .. v)
		end
		result = result .. value
		prev = after
	end

	return result .. pattern:sub(prev, #pattern)
end

function fill_template(buildpath, str, ctx)
	
	if type(str) == "number" then
		return tostring(str)
	end

	local result = {}
	local pos = 1
	
	while true do

		local at = str:find("@", pos, true)

		if at then
			if pos ~= at then
				result[#result + 1] = expand_pattern(str:sub(pos, at - 1), ctx)
			end
		else
			result[#result + 1] = expand_pattern(str:sub(pos), ctx)
			break
		end

		local next

		repeat
			local func, pattern
			func, pattern, next = str:match("^@expand%s+([A-Z_]+)%s+(.-)()\n", at)

			if func then
				-- TODO: Call func in ctx with the supplied pattern
				print("Expanding " .. func)
				local sub = ctx[func](pattern, ctx)
				result[#result + 1] = sub
				break
			end

			func, pattern, next = str:match("^@expand_block%s+([A-Z_]+)%s+(%b{})()", at)

			if func then
				-- TODO: Call func in ctx with the supplied pattern
				print("Expanding " .. func)
				local sub = ctx[func](pattern:sub(2, -2), ctx)
				result[#result + 1] = sub
				break
			end

			local path
			path, next = str:match("^@include%s+([^%s]+)()", at)

			if path then
				result[#result + 1] = readall(buildpath .. path)
				break
			end

			error("Unknown directive: " .. str:sub(at, at + 20) .. " ...")
		until true

		pos = next
	end

	return table.concat(result, '')
end

function mod.gen_bindings(buildpath, glpath, specfile)

	local core_spec = get_ext_set(buildpath .. "core", "GL_VERSION")
	local glx_core_spec = get_ext_set(buildpath .. "core", "GLX_VERSION")

	local ext_spec = get_ext_set(buildpath .. "extensions", "GL_")
	local glx_ext_spec = get_ext_set(buildpath .. "extensions", "GLX_")
	local wgl_ext_spec = get_ext_set(buildpath .. "extensions", "WGL_")

	local all_spec = merge_ext_sets(core_spec, ext_spec, wgl_ext_spec)

	-- TODO: Scan sources to find out which functions and constants are used.
	-- Include all constants and typedefs from used extensions.
	-- Include only those functions that are used in each extension.

	if false then
		local path = "../../gfx"

		print("These functions were found:")

		for f in lfs.dir(path) do
			if f:match("%.[ch]p?p?$") then
				local fullpath = path .. "/" .. f
				for word in readall(fullpath):gmatch("([_%w]+)") do
					local func = all_spec.func_name_to_func[word]

					if func then
						if not func.used then
							print(fullpath, word, func.ext.name)
						end
						func.used = true
						func.ext.used = true
					end

					local token = all_spec.token_name_to_token[word]

					if token then
						if not token.used then
							print(fullpath, word, token.ext.name)
						end
						token.used = true
						token.ext.used = true
					end
				end
			end
		end
	else
		local l = io.lines(specfile)

		for word in l do
			--local word = line:gsub("^%s*(.-)%s*$", "%1")
			local func = all_spec.func_name_to_func[word]
			local token = all_spec.token_name_to_token[word]
			local ext = all_spec.ext_name_to_ext[word]

			if func then
				if not func.used then
					print(word, func.ext.name)
				end
				func.used = true
				func.ext.used = true
			elseif token then
				if not token.used then
					print(word, token.ext.name)
				end
				token.used = true
				token.ext.used = true
			elseif ext then
				if not ext.used then
					print(word, ext.name)
				end
				ext.used = true
			elseif word ~= "" then
				print("Unrecognized name", word)
			end
		end
	end

	local all_spec_used = ext_set_filter_used(all_spec)
	local ext_used_count = #all_spec_used.list

	local table_size = 1
	local table_log2 = 0
	while table_size <= ext_used_count*2 do
		table_size = table_size * 2
		table_log2 = table_log2 + 1
	end

	local func_used_desc = {}
	local func_used_in_ext = {}

	for _,ext in ipairs(all_spec_used.list) do
		func_used_in_ext[ext] = 0
		for _,func in ipairs(ext.func_list) do
			if func.used then
				func_used_desc[#func_used_desc + 1] = func.name .. "\\0"
				func_used_in_ext[ext] = func_used_in_ext[ext] + 1
			end
		end
	end

	local function ext_func_used(pattern, ctx, prefix)
		local result = {}
		for fname,func in pairs(all_spec_used.func_name_to_func) do
			if func.ext.name:sub(1, #prefix) == prefix then
				ctx.NAME = fname
				local parms = func.parms
				parms = parms:gsub("HDC", "tl::win::HDC")
				ctx.TYPEDECL = func.rtype .. " (*PFN" .. fname:upper() .. "PROC)(" .. func.parms .. ")"
				ctx.WGL_TYPEDECL = func.rtype .. " (TL_WINAPI *PFN" .. fname:upper() .. "PROC)(" .. func.parms .. ")"
				ctx.TYPENAME = "PFN" .. fname:upper() .. "PROC"
				ctx.INDEX = all_spec_used.func_to_global_id[func]-1
				result[#result + 1] = fill_template(buildpath, pattern, ctx)
			end
		end
		return table.concat(result,"\n")
	end

	function ext_used(pattern, ctx, prefix)
		local result = {}
		for i,ext in ipairs(all_spec_used.list) do
			if ext.name:sub(1, #prefix) == prefix then
				ctx.NAME = ext.name
				ctx.INDEX = i-1
				result[#result + 1] = fill_template(buildpath, pattern, ctx)
			end
		end
		return table.concat(result,"\n")
	end

	local function ext_token_used(pattern, ctx, prefix)
		local result = {}
		for _,token in pairs(all_spec_used.token_name_to_token) do
			if token.ext.name:sub(1, #prefix) == prefix then
				--print("Filtered " .. token.name .. ", " .. token.ext.name)
				ctx.NAME = token.name
				ctx.VALUE = token.value
				result[#result + 1] = fill_template(buildpath, pattern, ctx)
			end
		end
		return table.concat(result,"\n")
	end

	local function ext_gl_versions_used(pattern, ctx)
		local result = {}
		for _,version in ipairs({
			'1_1', '1_2', '1_3', '1_4',
			'1_5', '2_0', '2_1', '3_1',
			'3_2', '3_3', '4_0'
		}) do
			local ext = all_spec_used.ext_name_to_ext['GL_VERSION_' .. version]

			if ext and ext.used then
				ctx.HEXVERSION = '0x' .. version:sub(1, 1) .. version:sub(3, 3)
				ctx.INDEX = all_spec_used.ext_to_id[ext] - 1
				result[#result + 1] = fill_template(buildpath, pattern, ctx)
			end
		end
		return table.concat(result,"\n")
	end

	local ctx = {
		EXT_USED_DESC = table.concat(map(all_spec_used.list, function(ext) return ext.string end), " "),
		TABLE_SIZE = table_size,
		TABLE_SHIFT = 32-table_log2,
		EXT_USED_COUNT = ext_used_count,
		FUNC_USED_COUNT = all_spec_used.func_count,

		FUNC_USED_DESC = table.concat(func_used_desc),

		FUNC_USED_PER_EXT = table.concat(map(all_spec_used.list, function(ext)
			print("Extension " .. ext.name .. ", used functions: " .. func_used_in_ext[ext])
			return tostring(func_used_in_ext[ext])
		end), ","),
		
		EXT_USED_WGL = function(pattern, ctx)
			return ext_used(pattern, ctx, "WGL_")
		end,

		EXT_USED_GL = function(pattern, ctx)
			return ext_used(pattern, ctx, "GL_")
		end,

		EXT_USED = function(pattern, ctx)
			return ext_used(pattern, ctx, "")
		end,

		EXT_FUNC_USED = function(pattern, ctx)
			return ext_func_used(pattern, ctx, "")
		end,

		EXT_FUNC_USED_GL = function(pattern, ctx)
			return ext_func_used(pattern, ctx, "GL_")
		end,

		EXT_FUNC_USED_WGL = function(pattern, ctx)
			return ext_func_used(pattern, ctx, "WGL_")
		end,

		EXT_TOKEN_USED_GL = function(pattern, ctx)
			return ext_token_used(pattern, ctx, "GL_")
		end,

		EXT_TOKEN_USED_WGL = function(pattern, ctx)
			return ext_token_used(pattern, ctx, "WGL_")
		end,

		EXT_MARK_GL_VERSION = function(pattern, ctx)
			return ext_gl_versions_used(pattern, ctx)
		end,
	}

	lfs.mkdir(glpath)

	writeall(glpath .. "/glew.c", fill_template(buildpath, readall(buildpath .. "template.c"), ctx))
	writeall(glpath .. "/glew.h", fill_template(buildpath, readall(buildpath .. "template.h"), ctx))
	writeall(glpath .. "/wglew.h", fill_template(buildpath, readall(buildpath .. "templatewgl.h"), ctx))
end

function mod.run(base, glpath, specfile)
	local buildpath = base .. 'gfx/_build/'

	memo.run('Generate gfx model', {
		buildpath .. 'gen.lua',
		buildpath .. 'template.c',
		buildpath .. 'template.h',
		buildpath .. 'templatewgl.h',
		specfile
	}, {
		glpath .. 'glew.c',
		glpath .. 'glew.h',
		glpath .. 'wglew.h'
	}, function()
		mod.gen_bindings(buildpath, glpath, specfile)
	end)
end

return mod