local char = ...

ffi.cdef [[

int MultiByteToWideChar(
  unsigned CodePage,
  uint32_t dwFlags,
  char const* lpMultiByteStr,
  int cbMultiByte,
  short* lpWideCharStr,
  int cchWideChar
);

int WideCharToMultiByte(
  unsigned CodePage,
  uint32_t dwFlags,
  short const* lpWideCharStr,
  int cchWideChar,
  char* lpMultiByteStr,
  int cbMultiByte,
  char const* lpDefaultChar,
  int* lpUsedDefaultChar
);

]]

local utf8 = 65001
local C = ffi.C
local cast = ffi.cast

local buffer_size = -1
local buffer

function char.str_to_utf16(str)
	local size = C.MultiByteToWideChar(65001, 0, str, #str, nil, 0)
	local sizeb = size*2
	if sizeb > buffer_size then
		buffer = ffi.new("char[?]", sizeb+2)
		buffer_size = sizeb
	end

	C.MultiByteToWideChar(65001, 0, str, #str, cast("short*", buffer), size)
	return ffi.string(buffer, sizeb)
end

function char.str_to_utf16_buf(str)
	local size = C.MultiByteToWideChar(65001, 0, str, #str, nil, 0)
	local b = ffi.new("short[?]", size+1)

	C.MultiByteToWideChar(65001, 0, str, #str, b, size)
	b[size] = 0
	return b
end

function char.utf16_to_str(utf16)
	local size16 = math.floor(#utf16 / 2)
	local size = C.WideCharToMultiByte(65001, 0, utf16, size16, nil, 0, nil, nil)

	if size > buffer_size then
		buffer = ffi.new("char[?]", size+1)
		buffer_size = size
	end

	C.WideCharToMultiByte(65001, 0, utf16, size16, buffer, size, nil, nil)
	return ffi.string(buffer, size)
end

function char.utf16_buf_to_str(utf16_buf, src_len)
	local diff = 0
	if not src_len then
		src_len = -1
		diff = -1
	end

	local size = C.WideCharToMultiByte(65001, 0, utf16_buf, src_len, nil, 0, nil, nil)

	if size > buffer_size then
		buffer = ffi.new("char[?]", size+1)
		buffer_size = size
	end

	C.WideCharToMultiByte(65001, 0, utf16_buf, src_len, buffer, size, nil, nil)
	return ffi.string(buffer, size + diff)
end

return char