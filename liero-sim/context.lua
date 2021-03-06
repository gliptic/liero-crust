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
				return 'ss::StructOffset<' .. ty.name .. 'Reader>' .. name
			elseif kind == 'expanded' then
				return ty.name .. name
			elseif kind == 'readerref' then
				return 'ss::Ref<' .. ty.name .. 'Reader>'
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
				f.var_type = var_type

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
			if kind == 'expanded' or kind == 'expanded' then
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
			if kind == 'expanded' then
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
					if kind == 'expanded' then
						return 'tl::VecSlice<' .. element_type.datatype(nil, kind) .. ' const>' .. name
					elseif kind == 'strict' then
						return 'ss::ArrayOffset<' .. element_type.datatype(nil, kind) .. '>' .. name
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

	compile_expand_programs = function(self)
		if self.expand_programs then
			return self.expand_programs
		end

		local programs = {}
		self.expand_programs = programs

		local program_refs = {}

		for _,t in ipairs(self.type_list) do
			if t.kind == 'Struct' then
				assert(t.program_pos == nil)

				local program_pos = #programs
				t.program_pos = program_pos

				local src_word_count = t.size >> 6
				local dest_word_count = src_word_count
				local op = src_word_count | (dest_word_count << 24)
				print('Gen program for', t.name, #programs, src_word_count, dest_word_count, op)
				programs[#programs + 1] = op

				local expand_flags_size = (src_word_count + 63) >> 6
				for i = 1,expand_flags_size do
					programs[#programs + 1] = 0
				end

				local default_pos = #programs + 1

				for i = 1,src_word_count do
					programs[#programs + 1] = t.defaults_block[i]
				end

				for _,f in ipairs(t.all_fields) do
					local word_pos = f.offset >> 6
					local op
					local info = 0

					if f.type.kind == 'String' then
						op = 3
					elseif f.type.kind == 'Array' then
						local element_type = f.type.element_type

						if element_type.kind == 'Struct' then
							op = 1
							program_refs[#program_refs + 1] = {
								from_struct = t,
								pos = default_pos + word_pos,
								struct = element_type
							}
						elseif element_type.kind == 'String' then
							op = 4
						else
							op = 2
							info = element_type.ref_size >> 3
						end
						
					elseif f.type.kind == 'Struct' then
						print('Struct field', t.name, f.type.name)
						op = 5
						program_refs[#program_refs + 1] = {
							from_struct = t,
							pos = default_pos + word_pos,
							struct = f.type
						}
					end

					if op ~= nil then
						programs[default_pos + word_pos] = (op << 32) | info
						programs[program_pos + 2 + (word_pos >> 6)] = programs[program_pos + 2 + (word_pos >> 6)] | (1 << (word_pos & 63))
					end
				end
			end
		end

		-- Fix references
		for _,r in ipairs(program_refs) do
			print(r.from_struct.name, r.struct.name)
			-- Relative to the second word of from_struct
			local rel_offs = r.struct.program_pos - (r.from_struct.program_pos + 1)
			local w = (programs[r.pos] & ~0xffffffff) | (rel_offs & 0xffffffff)
			print('Writing to', r.pos - 1)
			programs[r.pos] = w
		end

		return programs
	end,

	output_cpp = function(self, namespace, header_path, pr)
		pr('#include "', header_path, '"\n\n')

		pr('namespace ', namespace, ' {\n\n')

		local programs = self:compile_expand_programs()

		pr('u64 const expand_programs[' .. #programs .. '] = {')

		print('tc root:', programs[1 + 53])

		for i=0,#programs-1 do
			if (i & 3) == 0 then pr('\n', ii) end
			pr(programs[1 + i], ', ')
		end

		pr('\n};\n\n')

		--[[

		for _,t in ipairs(self.type_list) do
			if t.kind == 'Struct' then

				-- TODO: This needs to be aligned to 8 bytes
				pr('u8 ', t.name, '::_defaults[' .. (t.size >> 3) .. '] = {')

				for i = 0, (t.size >> 3) - 1 do
					local byte = (t.defaults_block[1 + (i >> 3)] >> ((i & 7) * 8)) & 0xff

					if (i & 7) == 0 then pr('\n', ii) end
					pr(byte, ', ')
				end

				pr('\n};\n\n')
			end
		end
		]]

		pr('}\n')
	end,

	output_hpp = function(self, namespace, header_name, pr)
		
		pr('#ifndef ', header_name, '_HPP\n')
		pr('#define ', header_name, '_HPP 1\n\n')
		pr('#include <tl/std.h>\n')
		pr('#include <tl/bits.h>\n')
		pr('#include "base_model.hpp"\n\n')

		local programs = self:compile_expand_programs()

		for _,v in ipairs(self.includes) do
			pr('#include ', v, '\n\n')
		end

		pr('namespace ', namespace, ' {\n\n')

		pr('extern u64 const expand_programs[', #programs, '];\n\n')

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

		local function def_types_for_field(f)
			if f.type.kind == 'StaticArray' then
				pr(ii, 'typedef ', f.var_type.datatype(f.name .. 'Val', 'expanded'), ';\n')
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

					--[[
					pr(ii, 'static u8 _defaults[' .. (t.size >> 3) .. '];\n\n')
					]]

					pr(ii, 'u8 data[', t.size >> 3, '];\n\n');

					--[[
					pr(ii, t.name, '() { memcpy(data, _defaults, sizeof(_defaults)); }\n')
					]]

					pr(ii, 'static u64 const* expand_program() { return expand_programs + ', t.program_pos, '; }\n')
					
					for _,f in ipairs(t.all_fields) do

						def_types_for_field(f)

						if f.type.kind == 'String' then
							pr('\n')

							pr(ii, 'tl::StringSlice ', f.name, '() const { ')
							pr('return this->_field<ss::StringOffset, ', f.offset >> 3, '>().get(); ')
							pr('}')
						elseif f.type.kind == 'Array' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'expanded')
							local element_datatype = f.type.element_type.datatype(nil, 'expanded')

							pr(ii, datatype, ' ', f.name, '() const { ')
							pr('return this->_field<ss::ArrayOffset<', element_datatype, '>, ', f.offset >> 3, '>().get(); ')
							pr('}')
						elseif f.type.kind == 'Struct' then
							pr('\n')

							local datatype = f.type.datatype(nil, 'expanded')

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

							local var_ref = 'this->_field<' .. f.var_type.datatype() .. ', ' .. (f.var_start >> 3) .. '>()'

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
							local rtype = f.type.datatype(nil, 'expanded')
							
							pr('\n')
							pr(ii, rtype, ' ', f.name, '() const { ')
							pr('return this->_field<', rtype, ', ', f.offset >> 3, '>(); ')
							pr('}')
						elseif f.type.kind == 'StaticArray' then

							local rtype = f.var_type.datatype(nil, 'expanded')

							--pr('\n')
							--pr(ii, 'typedef ', f.var_type.datatype(f.name .. 'Val', 'expanded'), ';\n')

							local var_ref = 'this->_field<' .. f.name .. 'Val const, ' .. (f.var_start >> 3) .. '>()'

							pr(ii, f.name, 'Val const& ', f.name, '() const { ')
							pr('return ', var_ref, '; ')
							pr('}')
						end
					end

					--[[
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
									element_type.datatype(nil, 'expanded'), ', ',
									element_type.datatype(nil, 'strict'),
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
									element_type.datatype(nil, 'expanded'), ', ',
									element_type.datatype(nil, 'strict'),
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
									element_type.datatype(nil, 'expanded'),
									'>, ', f.offset >> 3, '>().set(', f.name, '_copy);\n')
							end
						end

						pr(ii, '}\n')
					end

					]]

					pr('\n};\n')
				end

				do
					local basetype = t.datatype(nil, 'readerref')
					pr('\nstruct ', t.name, 'Builder : ', basetype, ' {\n')

					pr(ii, t.name, 'Builder(ss::Builder& b)\n')
					pr(iiii, ': Ref<', t.name, 'Reader>(b.alloc<', t.name, 'Reader>()) {\n')
					pr(ii, '}\n\n')

					pr(ii, t.name, 'Builder(ss::Ref<', t.name, 'Reader> b)\n')
					pr(iiii, ': Ref<', t.name, 'Reader>(b) {\n')
					pr(ii, '}\n\n')

					pr(ii, basetype, ' done() { return std::move(*this); }\n\n')

					for _,f in ipairs(t.all_fields) do

						def_types_for_field(f)

						local function def_set_for_field(f)
							
						end

						if f.type.kind == 'String' then
							pr(ii, 'void ', f.name, '(ss::StringRef v) { ')
							pr('return this->_field_ref<ss::StringOffset, ', f.offset >> 3, '>().set(v); ')
							pr('}\n')
						elseif f.type.kind == 'Struct' then
							local ref_datatype = f.type.datatype(nil, 'readerref')
							local strict_datatype = f.type.datatype(nil, 'strict')

							pr(ii, 'void ', f.name, '(', ref_datatype, ' v) { ')
							pr('return this->_field_ref<', strict_datatype, ', ', f.offset >> 3, '>().set(v); ')
							pr('}\n')
						elseif f.type.kind == 'Array' then

							local element_datatype = f.type.element_type.datatype(nil, 'strict')

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

							local var_ref = 'this->_field<' .. f.var_type.datatype() .. ', ' .. (f.var_start >> 3) .. '>()'

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = (', var_ref, ' & ', string.format('0x%x', ishmask), ') | ((v ^ ', default, ') << ', offset_in_var, '); ')
							pr('}\n')

						elseif f.type.kind == nil then

							local rtype = f.var_type.datatype()

							local var_ref = 'this->_field<' .. rtype .. ', ' .. (f.offset >> 3) .. '>()'
							local default = t.var_default(f.offset, f.size)

							pr(ii, 'void ', f.name, '(', rtype, ' v) { ')
							pr(var_ref, ' = v ^ ', default, '; ')
							pr('}\n')
						elseif f.type.kind == 'StaticArray' then

							local rtype = f.var_type.datatype(nil, 'expanded')

							local var_ref = 'this->_field<' .. f.name .. 'Val, ' .. (f.var_start >> 3) .. '>()'

							pr(ii, f.name, 'Val& ', f.name, '() { ')
							pr('return ', var_ref, '; ')
							pr('}\n')
						elseif f.type.kind == 'Float' then
							local storetype = f.type.datatype()
							local rtype = f.type.datatype(nil, 'expanded')

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
							local rtype = f.type.datatype(nil, 'expanded')
							
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
			if kind == 'strict' or kind == 'expanded' then
				return 'ss::StringOffset' .. name
			else
				return 'ss::StringRef' .. name
			end
		end },
		F64 = { kind = 'Float', ref_size = 64, align = 64, datatype = function(name, kind)
			if name then name = ' ' .. name else name = '' end
			if kind == 'expanded' or kind == 'strict' then
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

return {
	Context = Context
}