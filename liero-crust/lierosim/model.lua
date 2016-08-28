local memo = require "memo"

local ii = '\t'
local iiii = ii .. ii
local iiiiii = iiii .. ii

local Context_mt = {

	Struct = function(self, ty, ...)
		local cur_section = {}
		local fields = {}

		local function pack_section()
			for _,v in ipairs(cur_section) do
				local name, index, typename, default = v[1], v[2], v[3], v[4]

				local tt
				if type(typename) == "table" then
					tt = typename
				else
					tt = self.types[typename]
				end

				fields[#fields + 1] = {
					index = index,
					name = name,
					size = tt.ref_size,
					align = tt.align,
					type = tt,
					default = default
				}
			end

			table.sort(fields, function(a, b)
				return a.index < b.index
			end)
		end

		for _,m in ipairs({...}) do
			if m ~= 'Section' then
				cur_section[#cur_section + 1] = m
			end
		end

		pack_section()

		-- Allocate storage
		local free_space = {} -- Free space in terms of 64-bit words
		local padding_index = 0

		for _,f in ipairs(fields) do
			local word, offset = 1, 0
			local mask = 0xffffffffffffffff >> (64 - f.size)

			local step = f.align
				
			if f.size >= 64 and #free_space ~= 0 then
				-- Can only fit at the end
				word = #free_space
				mask = 0xffffffffffffffff
			end

			while word <= #free_space do
				local free = free_space[word]
					
				local shift_in = 0xffffffffffffffff
				if word == #free_space then
					shift_in = 0
				end

				while mask & ((free >> offset) | (shift_in << (64 - offset))) ~= 0 do
					offset = offset + step

					if offset + step > 64 then
						break
					end
				end

				if offset + step <= 64 then
					break
				end

				word = word + 1
				offset = 0
			end

			free_space[word] = (free_space[word] or 0) | (mask << offset)

			local left = f.size - (64 - offset)
			local i = 1

			while left > 0 do
				free_space[word + i] = 0xffffffffffffffff >> math.max(64 - left, 0)
				i = i + 1
				left = left - 64
			end

			f.offset = (word - 1) * 64 + offset
		end

		table.sort(fields, function(a, b)
			return a.offset < b.offset
		end)

		-- TODO: Move
		local function resolve_default(d)
			if type(d) == 'table' then
				return d.int
			else
				return d or 0
			end
		end

		local variables = {}
		local pos, padding_index = 0, 0

		local function pad_up_to(pad_to)
			while pos < pad_to do
				local pad_type = self.types.U8

				if pad_to - pos >= 32 and (pos & 31) == 0 then
					pad_type = self.types.U32
				elseif pad_to - pos >= 16 and (pos & 15) == 0 then
					pad_type = self.types.U16
				end

				local pad_size = pad_type.ref_size

				variables[#variables + 1] = {
					datatype = pad_type.datatype,
					name = '_padding' .. pos,
					start = pos,
					stop = pos + pad_size,
					default = 0
				}

				pos = pos + pad_size
			end
		end
		
		for _,f in ipairs(fields) do
		
			pad_up_to(f.offset)

			if pos == f.offset then
				local datatype, name, var_size
				if f.size < 8 then
					datatype = self.types.U8.datatype
					name = '_bits' .. pos
					var_size = 8
				else
					datatype = f.type.datatype
					name = f.name
					var_size = f.size
				end

				variables[#variables + 1] = {
					datatype = datatype,
					name = name,
					start = pos,
					stop = pos + var_size,
					default = 0
				}

				pos = pos + var_size
			end

			local v = variables[#variables]
			local mask = (0xffffffffffffffff >> math.max(64 - f.size, 0))
			local d = resolve_default(f.default) & mask
			v.default = v.default | (d << (f.offset - v.start))

			f.variable = v
		end

		local size, struct_alignment = pos, 64
		print(ty.name .. ': size = ' .. size .. ', struct_alignment = ' .. struct_alignment)

		if (size % struct_alignment) ~= 0 then
			size = size + struct_alignment - (size % struct_alignment)
		end

		--print(ty.name .. ': final size = ' .. size)

		pad_up_to(size)

		assert(not ty.kind)

		ty.kind = 'Struct'
		ty.ref_size = 64
		ty.align = 64
		ty.datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			if kind == 'strict' then
				return 'ss::StructRef<' .. ty.name .. 'Reader>' .. name
			elseif kind == 'expanded' then
				return ty.name .. name
			else
				return 'ss::Offset' .. name
			end
		end
		ty.size = pos
		ty.variables = variables
		ty.all_fields = fields

		return ty
	end,

	ExtType = function(self, ty, literal, size, alignment)
		assert(not ty.kind)

		ty.kind = 'ExtType'
		ty.ref_size = size
		ty.datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return literal .. name
		end
		ty.align = alignment
		ty.size = size
		
		return ty
	end,

	Enum = function(self, ty, base_type, ...)
		assert(not ty.kind)

		local values = {...}

		table.sort(values, function(a, b)
			return a[2] < b[2]
		end)

		ty.kind = 'Enum'
		ty.ref_size = assert(base_type.ref_size) -- TODO: Support lazy resolve?
		ty.align = assert(base_type.align)
		ty.datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return ty.name .. name
		end
		ty.base_type = base_type
		ty.size = ty.ref_size
		ty.values = values

		for k,v in ipairs(values) do
			ty[v[1]] = { literal = v[1], int = v[2] }
		end

		return ty
	end,
	
	Array = function(self, element_type)
		local existing = self.array_types[element_type]
		if not existing then
			existing = {
				kind = 'Array',
				ref_size = 64,
				align = 64,
				element_type = element_type,
				datatype = function(name, kind)
					if name then name = ' ' .. name else name = '' end
					if kind == 'strict' then
						return 'ss::ArrayRef<' .. element_type.datatype(nil, kind) .. '>' .. name
					elseif kind == 'expanded' then
						return 'ss::ArrayRef<' .. element_type.datatype(nil, kind) .. '>' .. name
					else
						return 'ss::Offset' .. name
					end
				end
			}
			self.array_types[element_type] = existing
		end
		return existing
	end,

	StaticArray = function(self, element_type, count)
		local total_size = element_type.ref_size * count

		return {
			kind = 'StaticArray',
			ref_size = total_size,
			align = element_type.align,
			element_type = type,
			datatype = function(name, kind)
				if name then name = ' ' .. name else name = '' end
				return element_type.datatype() .. name .. '[' .. count .. ']'
			end
		}
	end,

	output_cpp = function(self, namespace, header_path, pr)
		pr('#include "', header_path, '"\n\n')

		pr('namespace ', namespace, ' {\n\n')

		for name,t in pairs(self.types) do
			if t.kind == 'Struct' then

				-- TODO: This needs to be aligned to 8 bytes
				pr('u8 ', name, '::_defaults[' .. (t.size >> 3) .. '] = {')

				local count = 0
				for _,v in ipairs(t.variables) do

					local d, dsize = v.default, v.stop - v.start
					while dsize >= 8 do
						if (count & 7) == 0 then pr('\n', ii) end
						pr(d & 0xff, ', ')
						count = count + 1
						--if (count & 7) == 0 then pr('\n') end
						d = d >> 8
						dsize = dsize - 8
					end

					assert(dsize == 0) -- Variables must be multiplies of 8 bits
				end
				pr('\n};\n\n')
			end
		end

		pr('}\n')
	end,

	output_hpp = function(self, namespace, header_name, pr)
		
		pr('#ifndef ', header_name, '_HPP\n')
		pr('#define ', header_name, '_HPP 1\n\n')
		pr('#include <tl/std.h>\n')
		pr('#include <tl/bits.h>\n')
		pr('#include "base_model.hpp"\n\n')

		for _,v in ipairs(self.includes) do
			pr('#include ', v, '\n\n')
		end

		pr('namespace ', namespace, ' {\n\n')

		for _,t in ipairs(self.type_list) do
			if t.kind == 'Struct' then
				pr('struct ', t.name, ';\n')
			end
		end

		for _,t in ipairs(self.type_list) do
			if t.kind == 'Enum' then
				pr('\nenum struct ', t.name, ' : ', t.base_type.datatype(), ' {\n')
				for _,v in ipairs(t.values) do
					pr(ii, v[1], ' = ', v[2], ',\n')
				end
				pr('};\n')
			end
		end

		for _,t in ipairs(self.type_list) do
			if t.kind == 'Struct' then
				do
					pr('\nstruct alignas(8) ', t.name, 'Base {\n')

					pr(ii, 'static u8 _defaults[' .. (t.size >> 3) .. '];\n\n')

					for _,v in ipairs(t.variables) do
						pr(ii, v.datatype(v.name), ';\n')
					end

					-- Simple Accessors

					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'String' then
							pr('\n')
							pr(ii, 'tl::StringSlice ', f.name, '_unchecked() const {\n')
							pr(iiii, 'return this->', f.name, '.get();\n')
							pr(ii, '}\n')

							pr('\n')
							pr(ii, 'void ', f.name, '_unchecked(tl::StringSlice str) {\n')
							pr(iiii, 'this->', f.name, '.set(str);\n')
							pr(ii, '}\n')
						elseif f.type.ref_size < 8 then
							local rtype
							if f.type.ref_size == 1 then
								rtype = 'bool'
							else
								rtype = 'u8'
							end

							local var = f.variable
							local offset_in_var = f.offset - var.start
							local mask = (1 << f.size) - 1
							local shmask = mask << offset_in_var
							local varmask = (1 << (var.stop - var.start)) - 1
							local ishmask = ~shmask & varmask

							pr('\n')
							pr(ii, rtype, ' ', f.name, '() const {\n')
							pr(iiii, 'return ', rtype, '((this->', var.name, ' >> ', offset_in_var, ') & ', mask, ');\n')
							pr(ii, '}\n')

							pr('\n')
							pr(ii, 'void ', f.name, '(', rtype, ' v) {\n')
							pr(iiii, 'this->', var.name, ' = (this->', var.name, ' & ', string.format('0x%x', ishmask), ') | (v << ', offset_in_var, ');\n')
							pr(ii, '}\n')
						end
					end

					pr('};\n')

					pr('\nstatic_assert(sizeof(', t.name, 'Base) == ', t.size >> 3, ', "Unexpected size");\n')
				end

				-- Reader
				do

					pr('\nstruct ', t.name, 'Reader : ', t.name, 'Base {\n')

					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'Array' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'strict')
							pr(ii, datatype, '& ', f.name, '_unchecked() {\n')
							pr(iiii, 'return *(', datatype, '*)&this->', f.name, ';\n')
							pr(ii, '}\n')
						end
					end

					pr('};\n')
				end

				-- Expanded
				do
					pr('\nstruct ', t.name, ' : ', t.name, 'Base {\n')

					-- Accessors

					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'Array' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'expanded')
							pr(ii, datatype, '& ', f.name, '_unchecked() {\n')
							pr(iiii, 'return *(', datatype, '*)&this->', f.name, ';\n')
							pr(ii, '}\n')
						end
					end
				
					pr('\n')
					pr(ii, 'static void expand(ss::WriterRef<', t.name, '> dest, ss::Expander& expander, ss::StructRef<', t.name, 'Reader> const& src) {\n')
					pr(iiii, 'TL_UNUSED(expander);\n')
					pr(iiii, 'u32 src_size = src.size;\n')
					pr(iiii, t.name, 'Reader const* srcp = src.get();\n')
					
					if false then
						pr(iiii, 'u8 *p = (u8 *)&*dest;\n')
						pr(iiii, 'u8 const *s = (u8 const*)srcp;\n')
						pr(iiii, 'memcpy(p, _defaults, sizeof(', t.name, '));\n')
						pr(iiii, 'u32 written = tl::min(src_size, u32(sizeof(', t.name, ')));\n')
						pr(iiii, 'for (u32 i = 0; i < written; ++i) {\n')
						pr(iiiiii, 'p[i] ^= s[i];\n')
						pr(iiii, '}\n')
					else
						pr(iiii, 'u64 *p = (u64 *)&*dest;\n')
						pr(iiii, 'u64 const *s = (u64 const*)srcp;\n')
						pr(iiii, 'u64 const *d = (u64 const*)_defaults;\n')
						pr(iiii, 'for (u32 i = 0; i < ', t.size >> 6, '; ++i) {\n')
						pr(iiiiii, 'p[i] = d[i] ^ (i < src_size ? s[i] : 0);\n')
						pr(iiii, '}\n')
					end
										
					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'String' then
							pr(iiii, 'auto ', f.name, '_copy = expander.alloc_str(srcp->', f.name, '.get());\n')
						elseif f.type.kind == 'Array' then
							local element_type = f.type.element_type

							pr(iiii, 'auto ', f.name, '_copy = expander.expand_array<',
								element_type.datatype(nil, 'expanded'), ', ',
								element_type.datatype(nil, 'strict'),
								'>(srcp->', f.name, ');\n')
						end
					end

					local deref_done = false
					local function deref_dest()
						if not deref_done then
							deref_done = true
							pr(iiii, 'auto* destp = &*dest;\n')
						end
					end

					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'String' then
							deref_dest()
							pr(iiii, 'destp->', f.name, '_unchecked(*', f.name, '_copy);\n')
						elseif f.type.kind == 'Array' then
							deref_dest()
							pr(iiii, 'destp->', f.name, '.ptr(&*', f.name, '_copy);\n')
						end
					end

					pr(ii, '}\n')

					pr('};\n')
				end
			end
		end

		pr('}\n\n')
		pr('#endif\n')
	end
}

Context_mt.__index = Context_mt

local function Context()
	local ctx
	
	local types = {
		U32 = { ref_size = 32, align = 32, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'u32' .. name
		end },
		I32 = { ref_size = 32, align = 32, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'i32' .. name
		end },
		U16 = { ref_size = 16, align = 16, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'u16' .. name
		end },
		I16 = { ref_size = 16, align = 16, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'i16' .. name
		end },
		U8 = { ref_size = 8, align = 8, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'u8' .. name
		end },
		String = { kind = 'String', ref_size = 64, align = 64, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			return 'ss::StringRef' .. name
		end },
		Bit = { ref_size = 1, align = 1 }
	}
	local type_list = {}

	local types_mt = {
		__index = function (self, name)
			local ty = { name = name }
			self[name] = ty
			type_list[#type_list + 1] = ty
			return ty
		end
	}

	setmetatable(types, types_mt)

	ctx = setmetatable({
		includes = {},
		types = types,
		type_list = type_list,
		array_types = {}
	}, Context_mt)

	return ctx
end

local function generate(header_path, source_path)

	local ctx = Context()
	local t = ctx.types

	ctx.includes[#ctx.includes + 1] = '<tl/image.hpp>'

	ctx:ExtType(t.Scalar, 'Scalar', 32, 32)
	ctx:ExtType(t.Color, 'tl::Color', 32, 32)

	local function Scalar(x)
		return math.floor(x * 65536 + 0.5)
	end

	--ext:CheckedArrayIndex(t.NObjIdx, t.u16, )

	ctx:Enum(t.NObjectKind, t.U8,
		{'Normal', 0},
		{'DType1', 1},
		{'Steerable', 2},
		{'DType2', 3},
		{'Laser', 4})

	ctx:Enum(t.ScatterType, t.U8,
		{'ScAngular', 0},
		{'ScNormal', 1})

	ctx:Struct(t.WeaponType,
		{'name', 0, t.String},
		{'speed', 1, t.Scalar},
		{'distribution', 2, t.Scalar},
		{'worm_vel_ratio', 3, t.Scalar},

		{'parts', 4, t.U32},
		{'nobject_type', 5, t.U16},
		{'loading_time', 6, t.U32},
		{'fire_offset', 7, t.U32},

		{'ammo', 8, t.U32},
		{'delay', 9, t.U32},
		{'play_reload_sound', 10, t.Bit})

	ctx:Struct(t.WeaponTypeList,
		{'list', 0, ctx:Array(t.WeaponType)})

	ctx:Struct(t.NObjectType,
		{'gravity', 0, t.Scalar},
		{'splinter_count', 1, t.U32},
		{'splinter_type', 2, t.U16},
		{'splinter_scatter', 3, t.ScatterType, t.ScatterType.ScNormal},
		{'splinter_distribution', 4, t.Scalar},
		{'splinter_speed', 5, t.Scalar},
		
		{'time_to_live', 6, t.U32},
		{'time_to_live_v', 7, t.U32},

		{'sobj_trail_type', 8, t.U16, 0},
		{'sobj_trail_interval', 9, t.U32},
		{'sobj_trail_when_bounced', 31, t.Bit, 0},
		{'sobj_trail_when_hitting', 22, t.Bit},
		{'sobj_expl_type', 10, t.I16, -1},

		{'bounce', 11, t.Scalar, Scalar(-1)},
		{'friction', 12, t.Scalar, Scalar(1)},
		{'drag', 13, t.Scalar, Scalar(1)},
		{'blowaway', 14, t.Scalar},
		{'acceleration', 15, t.Scalar},

		{'start_frame', 16, t.I32},
		{'num_frames', 17, t.I32},
		{'colors', 18, ctx:StaticArray(t.Color, 2)},

		{'detect_distance', 19, t.U32},

		{'expl_ground', 20, t.Bit},
		{'directional_animation', 21, t.Bit, 1},
		{'collide_with_objects', 23, t.Bit},
		{'type', 24, t.NObjectKind},
		
		{'blood_trail_interval', 25, t.U32, 0},
		{'nobj_trail_interval', 26, t.U32, 0},
		{'nobj_trail_type', 27, t.U16, 0},
		{'nobj_trail_scatter', 28, t.ScatterType, t.ScatterType.ScNormal},
		{'nobj_trail_vel_ratio', 29, t.Scalar, Scalar(1/3)},
		{'nobj_trail_speed', 30, t.Scalar})

	ctx:Struct(t.SObjectType,
		{'anim_delay', 0, t.U32},
		{'start_frame', 1, t.U16},
		{'num_frames', 2, t.U16},
		{'detect_range', 3, t.U32},
		{'level_effect', 4, t.U16})

	ctx:Struct(t.TcData,
		{'nobjects', 0, ctx:Array(t.NObjectType)},
		{'sobjects', 1, ctx:Array(t.SObjectType)},
		{'weapons', 2, ctx:Array(t.WeaponType)},

		{'min_bounce_up', 3, t.Scalar})

	local namespace, header_name = 'liero', 'LIEROSIM_MODEL'

	do
		local f = io.open(header_path, "wb")
		ctx:output_hpp(namespace, header_name, function(...)
			for _,p in ipairs{...} do
				f:write(p)
			end
		end)
		f:close()
	end

	do
		local f = io.open(source_path, "wb")
		ctx:output_cpp(namespace, 'model.hpp', function(...)
			for _,p in ipairs{...} do
				f:write(p)
			end
		end)
		f:close()
	end
end

return {
	run = function(base, header_path, source_path)
		memo.run('Generate model', {base .. 'lierosim/model.lua'}, {header_path, source_path}, function()
			generate(header_path, source_path)
		end)
	end
}