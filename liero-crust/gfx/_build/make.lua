require "lfs"

function parse_ext(filename)
	local functions, tokens, types, exacts = {}, {}, {}, {}
	local extname, exturl, extstring

    local l = io.lines(filename)

	local extname = l()
	local exturl = l()
	local extstring = l()

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
					types[typename] = typevalue
					break
				end

				local tokenname, tokenvalue = line2:match("^([A-Z][A-Z0-9_x]*)%s+([0-9A-Za-z_]+)$")

				if tokenname then
					tokens[tokenname] = tokenvalue
					break
				end

				local funcreturn, funcname, funcparms =
					line2:match("^(.+) ([A-Za-z][A-Za-z0-9_]*) %((.+)%)$")

				if funcreturn then
					functions[funcname] = {rtype = funcreturn, parms = funcparms}
					break
				end

				error(line2 .. " matched no regexp")
			until false
		end
	end

	return extname, exturl, extstring, types, tokens, functions, exacts
end

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

function printlines(out, tab)
	for _,l in ipairs(tab) do
		out:write(l)
		out:write("\n")
	end
end

function readall(inpath)
	local f = io.open(inpath, "rb")
	local str = f:read("*a")
	f:close()
	return str
end

function appendfile(out, inpath)
	out:write(readall(inpath))
end

function keys(tab)
	local keytab = {}
	for k,_ in pairs(tab) do
		keytab[#keytab + 1] = k
	end
	return keytab
end

function output_tokens(out, tab, func)
	local k = keys(tab)
	if #k > 0 then
		out:write("\n")
		table.sort(k, function(a, b) return tab[a] < tab[b] end)
		printlines(out, map(k, function(key) return func(key, tab[key]) end))
		--out:write("\n")
	else
		--io.stderr:write("no keys in table!\n")
	end
end

function output_types(out, tab, func)
	local k = keys(tab)
	if #k > 0 then
		out:write("\n")
		table.sort(k, function(a, b) return tab[a] < tab[b] end)
		printlines(out, map(k, function(key) return func(key, tab[key]) end))
	end
end

function output_decls(out, tab, func)
	local k = keys(tab)
	if #k > 0 then
		out:write("\n")
		table.sort(k)
		printlines(out, mapi(k, function(key, i) return func(key, tab[key], i) end))
	end
end

function output_exacts(out, tab, func)
	if #tab > 0 then
		out:write("\n")
		table.sort(tab)
		printlines(out, map(tab, func))
	end
end

function prefixname(name)
    return name:gsub("^(.*)gl", "__%1glew", 1)
end

function exttablename(extname)
	return "__" .. extname .. "_IDX"
end

function indexedname(extname, idx)
    return exttablename(extname) .. "[" .. idx .. "]"
end

function prefix_varname(name)
	return name:gsub("^(.*)GL(X*)EW", "__%1GL%2EW")
end

function varname(extname)
	return extname:gsub("GL(X*)_", "GL%1EW_")
end

function make_define(key, value)
    return "#define " .. key .. " " .. value
end

function make_type(key, value)
    return "typedef " .. value .. " " .. key .. ";"
end

function make_exact(exact)
	exact = exact:gsub("; ", "; \n")
	exact = exact:gsub("{", "{\n")

    return exact
end

function make_pfn(name)
	return "PFN" .. name:upper() .. "PROC"
end

function make_pfn_decl(name)
	return make_pfn(name) .. " " .. prefixname(name) .. " = NULL;"
end

function make_separator(out, extname)
	local l = #extname
	local s = math.ceil((71 - l) / 2)

	out:write("/* ")

	local j = 3
	for i=0,s-1 do
		out:write("-")
		j = j + 1
	end

	out:write(" " .. extname .. " ")
	j = j + l + 2

	while j < 76 do
		out:write("-")
		j = j + 1
	end

	out:write(" */\n\n")
end

function make_header(out, api, type, extlist)

	table.sort(extlist)

	local function make_pfn_type(key, value)
		return "typedef " .. value.rtype .. " (" ..
			api .. " * PFN" .. key:upper() .. "PROC) " .. "(" .. value.parms .. ");"
	end



	local function make_init_decl(extname)
		out:write("GLboolean _glewInit_" .. extname .. " ();\n")
	end

	table.sort(extlist)

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local function make_pfn_alias(key, value, idx)
			return "#define " .. key .. " " .. type .. "EW_GET_FUN(_glewInit_" .. extname
				.. ", (" .. make_pfn(key) .. ")" .. indexedname(extname, idx) .. ")"
		end

		make_separator(out, extname)

		out:write("#ifndef " .. extname .. "\n#define " .. extname .. " 1\n")
		output_tokens(out, tokens, make_define)

		output_types(out, types, make_type);
		output_exacts(out, exacts, make_exact);
		output_decls(out, functions, make_pfn_type);
		output_decls(out, functions, make_pfn_alias);

		make_init_decl(extname)

		local extvar = varname(extname)

		out:write("\n#define " .. extvar .. " " .. type .. "EW_GET_VAR(_glewInit_" .. extname .. ", " ..
			prefix_varname(extvar) .. ")\n")

		out:write("\n#endif /* " .. extname .. " */\n\n")
	end
end

function make_def_fun(out, type, extlist)
	table.sort(extlist)

	out:write("\n")

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local k = keys(functions)
		
		if #k > 0 then
			out:write("void (*" .. exttablename(extname) .. "[" .. #k .. "])();\n")
		end
	end
end

function make_def_var(out, type, extlist)
	table.sort(extlist)

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local extvar = varname(extname)

		out:write("GLubyte " .. prefix_varname(extvar) .. ";\n")
	end
end


function make_init(out, type, extlist)
	table.sort(extlist)

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local first_func = true

		--[[
		function make_pfn_def_init(name)

			local ret = "  r = ((" .. prefixname(name) .. " = (PFN" .. name:upper() ..
				"PROC)glewGetProcAddress((const GLubyte*)\"" .. name .. "\")) != NULL) && r;"
			first_func = false
			return ret
		end]]

		function make_func_list(name)
			local ret = '"' .. name .. '",'
			return ret
		end

		out:write("#ifdef " .. extname .. "\n\n")

		local extvar = varname(extname)
		local pextvar = prefix_varname(extvar)
		local extvardef = extname

		local functionkeys = keys(functions)

		local has_functions = #functionkeys > 0

		out:write("GLboolean _glewInit_" .. extname .. " ()\n{\n  GLboolean r = GL_FALSE;\n")
		if has_functions then
			out:write("  int i;\n")
		end

		--[[
		if has_functions then
			out:write("static char const* _glewInitList_" .. extname .. "[] = {\n")
			output_decls(out, functions, make_func_list);
			out:write("};\n")
		end]]

		local initlist = "_glewInitList_" .. extname

		if has_functions then
			out:write("\n")
			table.sort(functionkeys)

			out:write('  char const* ' .. initlist .. ' = "')

			for _,n in ipairs(functionkeys) do
				out:write(n .. "\\0")
			end

			out:write('";')
		end

		out:write("  if (" .. pextvar .. ") return " .. pextvar .. " == 2;\n")

		if extstring == "" then
			extstring = extname
		end

		if type == "WGL" then
			if not next(functions) then
				out:write("  if (wgl_crippled) goto ret;\n")
			end
			out:write("  if (!wgl_crippled && !_glewFindString(\"" .. extstring .. "\")) goto ret;\n")
		else
			out:write("  if (!_glewFindString(\"" .. extstring .. "\")) goto ret;\n")
		end

		out:write("  r = GL_TRUE;\n")

		if has_functions then
			out:write("  for(i = 0; i < " .. #functionkeys .. "; ++i) {\n")
			out:write("    r = ((" .. indexedname(extname, "i") .. " = (void(*)())glewGetProcAddress((const GLubyte*)" ..
					"_glewInitList_" .. extname .. ")) != NULL) && r;\n")
			out:write("    " .. initlist .. " += _glewStrLen(" .. initlist .. ") + 1;\n")
			out:write("  }\n")
		end

		--output_decls(out, functions, make_pfn_def_init);

		out:write("ret:\n")
		out:write("  " .. pextvar .. " = r ? 2 : 1;\n")

		out:write("\n  return r;\n}\n\n")
		--end

		out:write("#endif /* " .. extname .. " */\n\n")
	end
end

function make_list(out, extlist)
	table.sort(extlist)

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local extvar = varname(extname)
		local extpre = extname:gsub("^(W?)GL(X?).*$", function(a, b)
			return a:lower() .. "gl" .. b:lower() .. "ew"
		end)

		out:write("#ifdef " .. extname .. "\n")

		if #extstring > 0 then

			out:write("  " .. prefix_varname(extvar) .. " = " .. extpre ..
				"GetExtension(\"" .. extstring .. "\");\n")
		end

		if next(functions) then
			if extname:match("WGL_") then
				out:write("  if (glewExperimental || " .. extvar ..
					"|| crippled) CONST_CAST(" .. extvar ..
					")= !_glewInit_" .. extname .. "(GLEW_CONTEXT_ARG_VAR_INIT);\n")
			else
				out:write("  if (glewExperimental || " .. extvar ..
					") CONST_CAST(" .. extvar .. ") = !_glewInit_" .. extname .. "(GLEW_CONTEXT_ARG_VAR_INIT);\n")
			end
		end

		out:write("#endif /* " .. extname .. " */\n")
	end
end

function make_str(out, extlist)
	table.sort(extlist)

	local curexttype = ""

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local _, _, exttype, extrem = extname:match("(W-)GL(X-)_(.-_)(.*)")
		local extvar = varname(extname)

		if exttype ~= curexttype then
			if #curexttype > 0 then
				out:write("      }\n")
			end
			out:write("      if (_glewStrSame2(&pos, &len, (const GLubyte*)\"" .. exttype ..
				"\", " .. #exttype .. "))\n")

			out:write("      {\n")
			curexttype = exttype
		end

		out:write("#ifdef " .. extname .. "\n")
		out:write("        if (_glewStrSame3(&pos, &len, (const GLubyte*)\"" .. extrem .. "\", " .. #extrem .. "))\n")
		out:write("        {\n")
		out:write("          ret = " .. extvar .. ";\n")
		out:write("          continue;\n")
		out:write("        }\n")
		out:write("#endif\n")
	end

	out:write("      }\n")
end

function make_struct_fun(out, export, extlist)
	function make_pfn_decl_exp(name)
		return export .. " PFN" .. name:upper() .. "PROC " .. prefixname(name) .. ";"
	end

	table.sort(extlist)

	out:write("\n")

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local k = keys(functions)

		if #k > 0 then
			out:write(export .. " void (*" .. exttablename(extname) .. "[" .. #k .. "])();\n")
		end
	end
end

function make_struct_var(out, export, extlist)
	table.sort(extlist)

	for _,ext in ipairs(extlist) do
		local extname, exturl, extstring, types, tokens, functions, exacts = parse_ext(ext)

		local extvar = varname(extname)

		out:write(export .. " GLboolean " .. prefix_varname(extvar) .. ";\n")
	end
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

local core_spec = getextlist("core", "GL_VERSION")
local glx_core_spec = getextlist("core", "GLX_VERSION")

local ext_spec = getextlist("extensions", "GL_")
local glx_ext_spec = getextlist("extensions", "GLX_")
local wgl_ext_spec = getextlist("extensions", "WGL_")

do
	local out = io.open("../GL/glew.h", "wb")

	appendfile(out, "src/glew_license.h")
	appendfile(out, "src/mesa_license.h")
	appendfile(out, "src/khronos_license.h")
	appendfile(out, "src/glew_head.h")

	make_header(out, "GLAPIENTRY", "GL", core_spec)
	make_header(out, "GLAPIENTRY", "GL", ext_spec)


	out:write([[
/* ------------------------------------------------------------------------- */

#if defined(GLEW_MX) && defined(_WIN32)
#define GLEW_FUN_EXPORT
#else
#define GLEW_FUN_EXPORT GLEWAPI
#endif /* GLEW_MX */
]])

	out:write([[
#if defined(GLEW_MX)
#define GLEW_VAR_EXPORT
#else
#define GLEW_VAR_EXPORT GLEWAPI
#endif /* GLEW_MX */
]])


	out:write([[
#if defined(GLEW_MX) && defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */
]])

	make_struct_fun(out, "GLEW_FUN_EXPORT", core_spec) -- TODO: Join
	make_struct_fun(out, "GLEW_FUN_EXPORT", ext_spec)

	out:write([[
#if defined(GLEW_MX) && !defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */
]])

	make_struct_var(out, "GLEW_VAR_EXPORT", core_spec) -- TODO: Join
	make_struct_var(out, "GLEW_VAR_EXPORT", ext_spec)

	out:write([[
#ifdef GLEW_MX
}; /* GLEWContextStruct */
#endif /* GLEW_MX */
]])


	appendfile(out, "src/glew_tail.h")

	--perl -e 's/GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_2;/GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_1;\nGLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_2;/' -pi $@
	--rm -f $@.bak

	out:close()
end

do
	local out = io.open("../GL/wglew.h", "wb")

	appendfile(out, "src/glew_license.h")
	appendfile(out, "src/khronos_license.h")
	appendfile(out, "src/wglew_head.h")

	make_header(out, "WINAPI", "WGL", wgl_ext_spec)

	out:write [[/* ------------------------------------------------------------------------- */

#ifdef GLEW_MX
#define WGLEW_EXPORT
#else
#define WGLEW_EXPORT GLEWAPI
#endif /* GLEW_MX */

#ifdef GLEW_MX
struct WGLEWContextStruct
{
#endif /* GLEW_MX */
]]

	make_struct_fun(out, "WGLEW_EXPORT", wgl_ext_spec)
	make_struct_var(out, "WGLEW_EXPORT", wgl_ext_spec)

	out:write [[
#ifdef GLEW_MX
}; /* WGLEWContextStruct */
#endif /* GLEW_MX */
]]


	appendfile(out, "src/wglew_tail.h")
end

do
	local out = io.open("../GL/glew.c", "wb")

	appendfile(out, "src/glew_license.h")
	appendfile(out, "src/glew_head.c")

	local min_tab_size = #core_spec + #ext_spec + #wgl_ext_spec + #glx_core_spec + #glx_ext_spec
	min_tab_size = min_tab_size * 1.5

	local tab_size = 2
	local tab_shift = 31
	while tab_size < min_tab_size do
		tab_size = tab_size * 2
		tab_shift = tab_shift - 1
	end

	out:write("\n#define _GLEW_STRING_TABLE_SIZE " .. tab_size)
	out:write("\n#define _GLEW_STRING_TABLE_SHIFT " .. tab_shift)

	out:write("\n\n#if !defined(_WIN32) || !defined(GLEW_MX)")

	make_def_fun(out, "GL", core_spec)
	make_def_fun(out, "GL", ext_spec)

	out:write("\n#endif /* !WIN32 || !GLEW_MX */")
	out:write("\n#if !defined(GLEW_MX)")
	out:write("\nGLboolean __GLEW_VERSION_1_1 = GL_FALSE;\n")

	make_def_var(out, "GL", core_spec)
	make_def_var(out, "GL", ext_spec)

	out:write("\n#endif /* !GLEW_MX */\n")

	make_init(out, "GL", core_spec)
	make_init(out, "GL", ext_spec)

	appendfile(out, "src/glew_init_gl.c")

	-- Initialization

	--[[
		make_list(out, core_spec)
		--$(BIN)/make_list.pl $(GL_CORE_SPEC) | grep -v '\"GL_VERSION' >> $@
		make_list(out, ext_spec)
	]]

	out:write([[
  return GLEW_OK;
}

#if defined(_WIN32)
#if !defined(GLEW_MX)
]])

	-- WGL function and variable definitions
	make_def_fun(out, "WGL", wgl_ext_spec)
	make_def_var(out, "WGL", wgl_ext_spec)

	out:write([[
#endif /* !GLEW_MX */
]])


	appendfile(out, "src/glew_init_wgl.c")

	--[[
	make_list(out, wgl_ext_spec)
	]]

	out:write([[
  return GLEW_OK;
}

]])

	-- WGL initialization functions
	make_init(out, "WGL", wgl_ext_spec)

	out:write([[
#elif !defined(__APPLE__) || defined(GLEW_APPLE_GLX)]])

	-- GLX function and variable definitions
	make_def_fun(out, "GLX", glx_core_spec)
	make_def_fun(out, "GLX", glx_ext_spec)

	out:write([[
#if !defined(GLEW_MX)
GLboolean __GLXEW_VERSION_1_0 = GL_FALSE;
GLboolean __GLXEW_VERSION_1_1 = GL_FALSE;]])

	make_def_var(out, "GLX", glx_core_spec)
	make_def_var(out, "GLX", glx_ext_spec)

	out:write("\n#endif /* !GLEW_MX */\n")

	-- GLX initialization functions
	make_init(out, "GLX", glx_core_spec)
	make_init(out, "GLX", glx_ext_spec)

	appendfile(out, "src/glew_init_glx.c")

	--[[
	make_list(out, {"core/GLX_VERSION_1_3"})
	--$(BIN)/make_list.pl $(CORE)/GLX_VERSION_1_3 | grep -v '\"GLX_VERSION' >> $@
	make_list(out, glx_ext_spec)
	]]

	out:write([[
  return GLEW_OK;
}
#endif /* !__APPLE__ || GLEW_APPLE_GLX */

]])

	appendfile(out, "src/glew_init_tail.c")
	appendfile(out, "src/glew_str_head.c")

	make_str(out, core_spec)
	make_str(out, ext_spec)

	appendfile(out, "src/glew_str_wgl.c")
	make_str(out, wgl_ext_spec)
	appendfile(out, "src/glew_str_glx.c")
	make_str(out, glx_core_spec)
	make_str(out, glx_ext_spec)
	appendfile(out, "src/glew_str_tail.c")

	out:close()
end

