local memo = require "memo"
local floatconv = require "floatconv"

local ii = '\t'
local iiii = ii .. ii
local iiiiii = iiii .. ii

local function resolve_default(d)
	if type(d) == 'table' then
		return d.int
	else
		return d or 0
	end
end

local Context_mt = {

	Struct = function(self, ty, ...)

		assert(not ty.kind)

		ty.kind = 'Struct'
		ty.ref_size = 64
		ty.align = 64
		ty.datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			if kind == 'strict' then
				return 'ss::StructRef<' .. ty.name .. 'Reader>' .. name
			elseif kind == 'strict2' then
				return 'ss::StructOffset<' .. ty.name .. 'Reader>' .. name
			elseif kind == 'expanded' then
				return ty.name .. name
			elseif kind == 'expanded2' then
				return ty.name .. name
			else
				return 'ss::Offset' .. name
			end
		end

		do -- TODO: Delay this block until its necessary
			local fields = {}
			local cur_scope = { children = {} }
			local cur_union = nil

			local function parse_field(v)
				local name, index, typename, default = v[1], v[2], v[3], v[4]

				local tt = typename
				assert(type(tt) == "table")
					
				if tt.kind == 'Float' then
					default = floatconv.doubleasint(default or 0)
				end

				--print('Union assignment: ' .. name .. ', ', cur_union)

				return {
					kind = 'Field',
					index = index,
					name = name,
					size = tt.ref_size,
					align = tt.align,
					type = tt,
					default = default,
					parent = cur_scope,
					union = cur_union,
					offset = -1
				}
			end

			local add_fields

			local function add_field(m)
				local t = m.t
				local node
				if t == 'Group' then
					local parent = cur_scope
					cur_scope = { kind = 'Group', name = m.name, parent = parent, children = {} }
					node = cur_scope
					add_fields(m)
					cur_scope = parent
				elseif t == 'Union' then
					fields[#fields + 1] = parse_field({m.name, m.index, m.ty})
					local parent, parent_union = cur_scope, cur_union
					cur_union = { kind = 'Union', name = m.name, parent = parent, children = {} }
					cur_scope = cur_union
					node = cur_union
					add_fields(m)
					cur_union = parent_union
					cur_scope = parent
				else
					node = parse_field(m)
					fields[#fields + 1] = node
				end

				cur_scope.children[#cur_scope.children + 1] = node

				return node
			end

			function add_fields(in_fields)
				for _,m in ipairs(in_fields) do
					add_field(m)
				end
			end

			add_fields({...})

			table.sort(fields, function(a, b)
				return a.index < b.index
			end)

			-- Allocate storage
			local free_space = {} -- Free space in terms of 64-bit words
			local defaults_block = {}

			local function mark_used(word, offset, mask, fsize, fdefault)
				local default = resolve_default(fdefault) & mask

				free_space[word] = (free_space[word] or 0) | (mask << offset)
				defaults_block[word] = (defaults_block[word] or 0) | (default << offset)

				local left = fsize - (64 - offset)
				local i = 1

				while left > 0 do
					free_space[word + i] = 0xffffffffffffffff >> math.max(64 - left, 0)
					defaults_block[word + 1] = 0

					i = i + 1
					left = left - 64
				end
			end

			local function exclude_tree(scope, word)
				if scope.kind == 'Field' and scope.offset >= 0 then
					local word_offset = (word - 1) * 64
					local upper = 0xffffffffffffffff << (scope.offset + scope.size - word_offset)
					local lower = 0xffffffffffffffff >> (64 - scope.offset + word_offset)

					return ~(upper | lower)
				elseif scope.children then
					local mask = 0
					for _,i in ipairs(scope.children) do
						mask = mask | exclude_tree(i, word)
					end

					return mask
				end

				return 0
			end

			local function exclude_impossible_fields(f, word)

				if f.union ~= nil then
					local cur = f
					local mask = 0
						
					while cur.parent do
						local parent = cur.parent

						if parent.kind == 'Union' then
							local submask = 0
							for _,i in ipairs(parent.children) do
								if i ~= cur then
									submask = submask | exclude_tree(i, word)
								end
							end

							mask = mask | (submask & ~exclude_tree(cur, word))
						end

						cur = parent
					end

					return ~mask
				else
					return ~0
				end
			end

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

					free = free & exclude_impossible_fields(f, word)
					
					-- TODO: If f is a field in a union, zero bits in free that belong to
					-- fields that cannot exist

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

				mark_used(word, offset, mask, f.size, f.default)

				f.offset = (word - 1) * 64 + offset
			end

			local pos = 0

			for _,f in ipairs(fields) do
				local var_type, var_size
				if f.size < 8 then
					var_type = self.types.U8
					var_size = 8
				else
					var_type = f.type
					var_size = f.size
				end

				local var_start = math.floor(f.offset / var_type.align) * var_type.align

				f.var_start = var_start
				f.var_stop = var_start + var_size
				f.var_datatype = var_type.datatype

				pos = math.max(pos, f.var_stop)
			end

			local size, struct_alignment = pos, 64
			print(ty.name .. ': size = ' .. size .. ', struct_alignment = ' .. struct_alignment)

			if (size % struct_alignment) ~= 0 then
				size = size + struct_alignment - (size % struct_alignment)
			end

			ty.size = size
			ty.all_fields = fields
			ty.defaults_block = defaults_block
			ty.var_default = function(offset, size)
				assert(size <= 64)
				local mask = (1 << size) - 1
				return (defaults_block[1 + (offset >> 6)] >> (offset & 63)) & mask
			end
		end

		return ty
	end,

	ExtType = function(self, ty, literal, base_type, conv_to, conv_from)
		assert(not ty.kind)

		ty.kind = 'ExtType'
		ty.ref_size = assert(base_type.ref_size)
		ty.datatype = function(name, kind)
			if kind == 'expanded' or kind == 'expanded2' then
				if name then name = ' ' .. name else name = '' end
				return literal .. name
			end
			return base_type.datatype(name, kind)
		end
		ty.literal = literal
		ty.base_type = base_type
		ty.conv_to = conv_to
		ty.conv_from = conv_from
		ty.align = assert(base_type.align)
		ty.size = ty.ref_size
		
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
		ty.datatype = function(name, kind)
			if kind == 'expanded' or kind == 'expanded2' then
				if name then name = ' ' .. name else name = '' end
				return ty.name .. name
			end
			return base_type.datatype(name, kind)
		end
		ty.base_type = base_type
		ty.size = ty.ref_size
		ty.align = assert(base_type.align)
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
					if kind == 'expanded2' then
						return 'tl::VecSlice<' .. element_type.datatype(nil, kind) .. ' const>' .. name
					elseif kind == 'strict2' then
						return 'ss::ArrayOffset<' .. element_type.datatype(nil, kind) .. '>' .. name
					elseif kind == 'strict' or kind == 'expanded' then
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
				return element_type.datatype(nil, kind) .. name .. '[' .. count .. ']'
			end
		}
	end,

	Group = function(self, name, ...)
		return {t = 'Group', name = name, ...}
	end,

	Union = function(self, name, index, ty, ...)
		return {t = 'Union', name = name, index = index, ty = ty, ...}
	end,

	output_cpp = function(self, namespace, header_path, pr)
		pr('#include "', header_path, '"\n\n')

		pr('namespace ', namespace, ' {\n\n')

		for name,t in pairs(self.types) do
			if t.kind == 'Struct' then

				-- TODO: This needs to be aligned to 8 bytes
				pr('u8 ', name, '::_defaults[' .. (t.size >> 3) .. '] = {')

				for i = 0, (t.size >> 3) - 1 do
					local byte = (t.defaults_block[1 + (i >> 3)] >> ((i & 7) * 8)) & 0xff

					if (i & 7) == 0 then pr('\n', ii) end
					pr(byte, ', ')
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
					pr('\nstruct alignas(8) ', t.name, 'Reader : ss::Struct {\n')

					pr(ii, 'u8 data[', t.size >> 3, '];\n');

					pr('\n};\n')
				end

				do
					pr('\nstruct alignas(8) ', t.name, ' : ss::Struct {\n')

					pr(ii, 'static u8 _defaults[' .. (t.size >> 3) .. '];\n\n')

					pr(ii, 'u8 data[', t.size >> 3, '];\n\n');

					pr(ii, t.name, '() { memcpy(data, _defaults, sizeof(_defaults)); }\n')

					for _,f in ipairs(t.all_fields) do

						if f.type.kind == 'String' then
							pr('\n')

							pr(ii, 'tl::StringSlice ', f.name, '() const { ')
							pr('return this->_field<ss::StringOffset, ', f.offset >> 3, '>().get(); ')
							pr('}')
						elseif f.type.kind == 'Array' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'expanded2')
							local element_datatype = f.type.element_type.datatype(nil, 'expanded2')

							pr(ii, datatype, ' ', f.name, '() const { ')
							pr('return this->_field<ss::ArrayOffset<', element_datatype, '>, ', f.offset >> 3, '>().get(); ')
							pr('}')
						elseif f.type.kind == 'Struct' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'expanded2')

							pr(ii, datatype, ' const& ', f.name, '() const { ')
							pr('return *this->_field<ss::StructOffset<', datatype, '>, ', f.offset >> 3, '>().get(); ')
							pr('}')
						elseif f.type.ref_size < 8 then
							local rtype
							if f.type.ref_size == 1 then
								rtype = 'bool'
							else
								rtype = 'u8'
							end

							local offset_in_var = f.offset - f.var_start
							local mask = (1 << f.size) - 1
							local shmask = mask << offset_in_var
							local varmask = (1 << (f.var_stop - f.var_start)) - 1
							local ishmask = ~shmask & varmask

							local var_ref = 'this->_field<' .. f.var_datatype() .. ', ' .. (f.var_start >> 3) .. '>()'

							pr('\n')
							if f.type.ref_size == 1 then
								pr(ii, rtype, ' ', f.name, '() const { ')
								pr('return (', var_ref, ' & ', shmask, ') != 0; ')
							else
								pr(ii, rtype, ' ', f.name, '() const { ')
								pr('return ', rtype, '((', var_ref, ' >> ', offset_in_var, ') & ', mask, '); ')
							end
							pr('}')

						elseif f.type.kind == nil or f.type.kind == 'Float' or f.type.kind == 'ExtType' or f.type.kind == 'Enum' then
							local rtype = f.type.datatype(nil, 'expanded2')
							
							pr('\n')
							pr(ii, rtype, ' ', f.name, '() const { ')
							pr('return this->_field<', rtype, ', ', f.offset >> 3, '>(); ')
							pr('}')
						elseif f.type.kind == 'StaticArray' then

							local rtype = f.var_datatype(nil, 'expanded2')

							pr(ii, 'typedef ', f.var_datatype(f.name .. 'Val', 'expanded2'), ';\n')

							local var_ref = 'this->_field<' .. f.name .. 'Val const, ' .. (f.var_start >> 3) .. '>()'

							pr(ii, f.name, 'Val const& ', f.name, '() const { ')
							pr('return ', var_ref, '; ')
							pr('}\n')
						end
					end

					do
						pr('\n\n')
						pr(ii, 'static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<', t.name, 'Reader> const& src) {\n')
						pr(iiii, 'TL_UNUSED(expander); TL_UNUSED(src);\n')

						local deref_done = false
						local function deref_src()
							if not deref_done then
								deref_done = true
								pr(iiii, t.name, 'Reader const* srcp = src.get();\n')
							end
						end

						for _,f in ipairs(t.all_fields) do

							if f.type.kind == 'String' then
								deref_src()
								pr(iiii, 'cur_size += ss::round_size_up(srcp->_field<ss::StringOffset, ', f.offset >> 3, '>().size);\n')
							elseif f.type.kind == 'Struct' then
								deref_src()
								pr(iiii, 'cur_size = ', f.type.name, '::calc_extra_size(', f.type.size >> 3, ' + cur_size, expander, srcp->_field<ss::StructOffset<', f.type.name, 'Reader> const, ', f.offset >> 3, '>());\n')
							elseif f.type.kind == 'Array' then
								local element_type = f.type.element_type
								
								deref_src()

								local func = 'array_calc_size_plain'
								if element_type.kind == 'Struct' or element_type.kind == 'String' then
									func = 'array_calc_size'
								end

								pr(iiii, 'cur_size = expander.', func, '<',
									element_type.datatype(nil, 'expanded2'), ', ',
									element_type.datatype(nil, 'strict2'),
									'>(cur_size, srcp->_field<ss::Offset, ', f.offset >> 3, '>());\n')
							end
						end

						pr(iiii, 'return cur_size;\n')

						pr(ii, '}\n')
					end
					
					do
						pr('\n')
						pr(ii, 'static void expand_raw(ss::Ref<', t.name, '> dest, ss::Expander& expander, ss::StructOffset<', t.name, 'Reader> const& src) {\n')
						pr(iiii, 'TL_UNUSED(expander);\n')
						pr(iiii, 'u32 src_size = src.size;\n')
						pr(iiii, t.name, 'Reader const* srcp = src.get();\n')
					
						pr(iiii, 'u64 *p = (u64 *)dest.ptr;\n')
						pr(iiii, 'u64 const *s = (u64 const*)srcp;\n')
						pr(iiii, 'u64 const *d = (u64 const*)', t.name, '::_defaults;\n')
						pr(iiii, 'for (u32 i = 0; i < ', t.size >> 6, '; ++i) {\n')
						pr(iiiiii, 'p[i] = d[i] ^ (i < src_size ? s[i] : 0);\n')
						pr(iiii, '}\n')
		
						for _,f in ipairs(t.all_fields) do

							if f.type.kind == 'String' then
								pr(iiii, 'auto ', f.name, '_copy = expander.alloc_str_raw(srcp->_field<ss::StringOffset, ', f.offset >> 3, '>().get());\n')
							elseif f.type.kind == 'Struct' then
								pr(iiii, 'auto ', f.name, '_copy = expander.alloc_uninit_raw<', f.type.name, '>();\n')
								pr(iiii, f.type.name, '::expand_raw(', f.name, '_copy, expander, srcp->_field<ss::StructOffset<', f.type.name, 'Reader>, ', f.offset >> 3, '>());\n')
							elseif f.type.kind == 'Array' then
								local element_type = f.type.element_type

								local func = 'expand_array_raw_plain'
								if element_type.kind == 'Struct' or element_type.kind == 'String' then
									func = 'expand_array_raw'
								end

								pr(iiii, 'auto ', f.name, '_copy = expander.', func, '<',
									element_type.datatype(nil, 'expanded2'), ', ',
									element_type.datatype(nil, 'strict2'),
									'>(srcp->_field<ss::Offset, ', f.offset >> 3, '>());\n')
							end
						end

						for _,f in ipairs(t.all_fields) do
							if f.type.kind == 'String' then
								pr(iiii, 'dest._field_ref<ss::StringOffset, ', f.offset >> 3, '>().set(', f.name, '_copy);\n')
							elseif f.type.kind == 'Struct' then
								pr(iiii, 'dest._field_ref<ss::StructOffset<', f.type.name, '>, ', f.offset >> 3, '>().set(', f.name, '_copy);\n')
							elseif f.type.kind == 'Array' then
								local element_type = f.type.element_type

								pr(iiii, 'dest._field_ref<ss::ArrayOffset<',
									element_type.datatype(nil, 'expanded2'),
									'>, ', f.offset >> 3, '>().set(', f.name, '_copy);\n')
							end
						end

						pr(ii, '}\n')
					end

					pr('\n};\n')
				end

				do
					pr('\nstruct ', t.name, 'Builder : ss::Ref<', t.name, 'Reader> {\n')

					pr(ii, t.name, 'Builder(ss::Builder& b)\n')
					pr(iiii, ': Ref<', t.name, 'Reader>(b.alloc<', t.name, 'Reader>()) {\n')
					pr(ii, '}\n\n')

					pr(ii, 'Ref<', t.name, 'Reader> done() { return std::move(*this); }\n\n')

					for _,f in ipairs(t.all_fields) do
						if f.type.kind == 'String' then
							pr(ii, 'void ', f.name, '(ss::StringRef v) { ')
							pr('return this->_field_ref<ss::StringOffset, ', f.offset >> 3, '>().set(v); ')
							pr('}\n')
						elseif f.type.kind == 'Array' then

							local datatype = f.type.datatype(nil, 'expanded2')
							local element_datatype = f.type.element_type.datatype(nil, 'strict2')

							pr(ii, 'void ', f.name, '(ss::ArrayRef<', element_datatype, '> v) { ')
							pr('return this->_field_ref<ss::ArrayOffset<', element_datatype, '>, ', f.offset >> 3, '>().set(v); ')
							pr('}\n')
						elseif f.type.ref_size < 8 then
							local rtype
							if f.type.ref_size == 1 then
								rtype = 'bool'
							else
								rtype = 'u8'
							end

							local offset_in_var = f.offset - f.var_start
							local mask = (1 << f.size) - 1
							local shmask = mask << offset_in_var
							local varmask = (1 << (f.var_stop - f.var_start)) - 1
							local ishmask = ~shmask & varmask

							local default = t.var_default(f.offset, f.size)

							local var_ref = 'this->_field<' .. f.var_datatype() .. ', ' .. (f.var_start >> 3) .. '>()'

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = (', var_ref, ' & ', string.format('0x%x', ishmask), ') | ((v ^ ', default, ') << ', offset_in_var, '); ')
							pr('}\n')

						elseif f.type.kind == nil then

							local rtype = f.var_datatype()

							local var_ref = 'this->_field<' .. rtype .. ', ' .. (f.offset >> 3) .. '>()'
							local default = t.var_default(f.offset, f.size)

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = v ^ ', default, '; ')
							pr('}\n')
						elseif f.type.kind == 'StaticArray' then

							local rtype = f.var_datatype(nil, 'expanded2')

							pr(ii, 'typedef ', f.var_datatype(f.name .. 'Val', 'expanded2'), ';\n')

							local var_ref = 'this->_field<' .. f.name .. 'Val, ' .. (f.var_start >> 3) .. '>()'

							pr(ii, f.name, 'Val& ', f.name, '() { ')
							pr('return ', var_ref, '; ')
							pr('}\n')
						elseif f.type.kind == 'Float' then
							local storetype = f.type.datatype()
							local rtype = f.type.datatype(nil, 'expanded2')

							local var_ref = 'this->_field<' .. storetype .. ', ' .. (f.var_start >> 3) .. '>()'
							local default = t.var_default(f.offset, f.size)

							pr('\n')
							pr(ii, 'void ', f.name, '(', rtype, ' v) {\n')
							pr(iiii, storetype, ' s;\n')
							pr(iiii, 'memcpy(&s, &v, sizeof(v));\n')
							pr(iiii, var_ref, ' = s ^ ', default, ';\n')
							pr(ii, '}\n')
						elseif f.type.kind == 'ExtType' then
							local storetype = f.type.datatype()
							local rtype = f.type.literal

							local var_ref = 'this->_field<' .. storetype .. ', ' .. (f.var_start >> 3) .. '>()'
							local default = t.var_default(f.offset, f.size)

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = (', f.type.conv_from('v'), ') ^ ', default, '; ')
							pr('}\n')
						elseif f.type.kind == 'Enum' then
							local storetype = f.type.datatype()
							local rtype = f.type.datatype(nil, 'expanded2')
							
							local var_ref = 'this->_field<' .. storetype .. ', ' .. (f.var_start >> 3) .. '>()'
							local default = t.var_default(f.offset, f.size)

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = ', f.type.datatype(), '(v) ^ ', default, '; ')
							pr('}\n')
						end
					end

					pr('\n};\n')
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
			if kind == 'strict2' or kind == 'expanded2' then
				return 'ss::StringOffset' .. name
			else
				return 'ss::StringRef' .. name
			end
		end },
		F64 = { kind = 'Float', ref_size = 64, align = 64, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			if kind == 'expanded' or kind == 'expanded2' or kind == 'strict' or kind == 'strict2' then
				return 'f64' .. name
			else
				return 'u64' .. name
			end
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

	ctx.includes[#ctx.includes + 1] = '<tl/gfx/image.hpp>'

	ctx:ExtType(t.Scalar, 'Scalar', t.I32, function(v)
		return 'Scalar::from_raw(' .. v .. ')'
	end, function(v)
		return v .. '.raw()'
	end)

	ctx:ExtType(t.Color, 'tl::Color', t.U32, function(v)
		return 'tl::Color(' .. v .. ')'
	end, function(v)
		return v .. '.v'
	end)

	local function Scalar(x)
		return math.floor(x * 65536 + 0.5)
	end

	local function RatioFromFixed(x)
		return x / 65536
	end

	local function Ratio(x, y)
		return x / (y or 1)
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

	ctx:Enum(t.NObjectAnimation, t.U8,
		{'TwoWay', 0},
		{'OneWay', 1},
		{'Static', 2})

	ctx:Struct(t.WeaponType,
		{'name', 0, t.String},
		{'speed', 1, t.F64},
		{'distribution', 2, t.F64},
		{'worm_vel_ratio', 3, t.F64},

		{'parts', 4, t.U32},
		{'nobject_type', 5, t.U16},
		{'loading_time', 6, t.U32},
		{'fire_offset', 7, t.U32},
		{'fire_sound', 12, t.I16},
		{'recoil', 11, t.F64},
		{'muzzle_fire', 13, t.U8},

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
		{'splinter_distribution', 4, t.F64},
		{'splinter_speed', 46, t.F64},
		{'splinter_speed_v', 5, t.F64},
		{'splinter_color', 32, t.Color},
		
		{'time_to_live', 6, t.U32},
		{'time_to_live_v', 7, t.I32},

		{'sobj_trail_type', 8, t.U16, 0},
		{'sobj_trail_interval', 9, t.U32},
		{'sobj_trail_when_bounced', 33, t.Bit, 0},
		{'sobj_trail_when_hitting', 22, t.Bit},
		{'sobj_expl_type', 10, t.I16, -1},
		{'expl_sound', 40, t.I16, -1},

		{'bounce', 11, t.F64, Ratio(-1)},
		{'friction', 12, t.F64, Ratio(1)},
		{'drag', 13, t.F64, Ratio(1)},
		{'blowaway', 14, t.F64},
		{'acceleration', 15, t.F64},
		{'acceleration_up', 39, t.F64},
		{'affect_by_sobj', 37, t.Bit},

		{'start_frame', 16, t.I32},
		{'num_frames', 17, t.I32},
		{'colors', 18, ctx:StaticArray(t.Color, 2)},

		{'detect_distance', 19, t.U32},
		{'hit_damage', 35, t.U16},
		{'worm_coldet', 41, t.Bit},
		{'worm_col_remove_prob', 42, t.U32},
		{'worm_col_expl', 43, t.Bit},
		{'worm_col_blood', 44, t.U32},

		{'expl_ground', 20, t.Bit},
		{'draw_on_level', 38, t.Bit},
		{'level_effect', 36, t.I16, -1},
		{'animation', 34, t.NObjectAnimation, t.NObjectAnimation.TwoWay},
		{'collide_with_objects', 23, t.Bit},
		{'kind', 24, t.NObjectKind},
		
		{'blood_trail_interval', 25, t.U32, 0},
		{'nobj_trail_interval', 26, t.U32, 0},
		{'nobj_trail_type', 27, t.U16, 0},
		{'nobj_trail_scatter', 28, t.ScatterType, t.ScatterType.ScNormal},
		{'nobj_trail_vel_ratio', 29, t.F64, Ratio(1, 3)},
		{'nobj_trail_speed', 30, t.F64},
		{'nobj_trail_interval_inv', 45, t.U32, 0},
		
		{'physics_speed', 31, t.U8, 1})

		-- 46

	ctx:Struct(t.SObjectType,
		{'anim_delay', 0, t.U32},
		{'start_frame', 1, t.U16},
		{'num_frames', 2, t.U16},
		{'detect_range', 3, t.U32},
		{'level_effect', 4, t.I16, -1},
		{'start_sound', 5, t.I16, -1},
		{'worm_blow_away', 7, t.Scalar},
		{'nobj_blow_away', 8, t.Scalar},
		{'damage', 9, t.U32},
		{'num_sounds', 6, t.U8})

	ctx:Struct(t.LevelEffect,
		{'mframe', 0, t.U32},
		{'rframe', 1, t.U32},
		{'sframe', 2, t.U32},
		{'draw_back', 3, t.Bit})

	ctx:Struct(t.TcData,
		{'nobjects', 0, ctx:Array(t.NObjectType)},
		{'sobjects', 1, ctx:Array(t.SObjectType)},
		{'weapons', 2, ctx:Array(t.WeaponType)},
		{'level_effects', 3, ctx:Array(t.LevelEffect)},
		{'sound_names', 73, ctx:Array(t.String)},

		{'nr_initial_length', 5, t.F64, (4000 / 16)},
		{'nr_attach_length', 6, t.F64, (450 / 16)},
		{'nr_min_length', 7, t.F64, (170 / 16)},
		{'nr_max_length', 8, t.F64, (4000 / 16)},
		{'nr_throw_vel', 9, t.F64, 4},
		{'nr_force_mult', 12, t.F64},

		{'nr_pull_vel', 17, t.F64},
		{'nr_release_vel', 18, t.F64},
		{'nr_colour_begin', 19, t.U8},
		{'nr_colour_end', 20, t.U8},
		{'min_bounce', 24, t.Scalar, 53248},
		{'worm_gravity', 25, t.Scalar, 1500},
		{'walk_vel', 26, t.Scalar, 3000},
		{'max_vel', 27, t.Scalar, 29184},
		{'jump_force', 28, t.Scalar, 56064},
		{'max_aim_vel', 29, t.Scalar, 70000},
		{'aim_acc', 30, t.Scalar, 4000},
		{'aim_fric_mult', 41, t.F64, RatioFromFixed(54394)},
		{'aim_max_right', 53, t.U32, 116},
		{'aim_min_right', 54, t.U32, 64},
		{'aim_max_left', 55, t.U32, 12},
		{'aim_min_left', 56, t.U32, 64},
		{'ninjarope_gravity', 31, t.Scalar},
		{'bonus_gravity', 32, t.Scalar},
		{'worm_fric_mult', 33, t.F64, RatioFromFixed(58327)},
		{'worm_min_spawn_dist_last', 35, t.U32},
		{'worm_min_spawn_dist_enemy', 36, t.U32},
		{'bonus_bounce_mult', 42, t.F64},
		{'bonus_flicker_time', 44, t.U32},
		{'bonus_explode_risk', 45, t.Scalar},
		{'bonus_health_var', 46, t.U32},
		{'bonus_min_health', 47, t.U32},
		{'bonus_drop_chance', 52, t.Scalar},
		{'first_blood_colour', 58, t.U8, 80},
		{'num_blood_colours', 59, t.U8, 2},
		{'bobj_gravity', 60, t.Scalar, 1000},
		{'blood_step_up', 63, t.U32},
		{'blood_step_down', 64, t.U32},
		{'blood_limit', 65, t.U32},
		{'fall_damage_right', 66, t.U32},
		{'fall_damage_left', 67, t.U32},
		{'fall_damage_down', 68, t.U32},
		{'fall_damage_up', 69, t.U32},
		{'worm_float_level', 70, t.U32},
		{'worm_float_power', 71, t.Scalar},
		{'rem_exp_object', 72, t.I16},
		{'materials', 73, ctx:Array(t.U8, 256)},
		{'throw_sound', 74, t.U8})

	ctx:Struct(t.PlayerControls,
		{'up', 0, t.U16},
		{'down', 1, t.U16},
		{'left', 2, t.U16},
		{'right', 3, t.U16},
		{'fire', 4, t.U16},
		{'change', 5, t.U16},
		{'jump', 6, t.U16})

	ctx:Struct(t.PlayerSettings,
		{'left_controls', 0, t.PlayerControls},
		{'right_controls', 1, t.PlayerControls})

	--[[
	ctx:Struct(t.Node,
		{'name', 0, t.String},
		ctx:Union('kind', 3, t.Bit,
			ctx:Group('file',
				{'contents', 1, ctx:Array(t.U8)},
				{'offset_in_contents', 4, t.U32}),
			{'subnodes', 2, ctx:Array(t.Node)}))

	ctx:Struct(t.Archive,
		{'tree', 0, t.Node})
	]]
	
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
		memo.run('Generate model', {base .. 'data/model.lua'}, {header_path, source_path}, function()
			generate(header_path, source_path)
		end)
	end
}