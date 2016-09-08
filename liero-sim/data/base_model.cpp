#include "base_model.hpp"

namespace ss {

/*
WriterRef<u64> Expander::expand_array_(Offset const& from, usize struct_words, StructExpand struct_expand) {
	auto p = (Offset const *)from.ptr();

	WriterRef<u64> new_arr = this->alloc_uninit<u64>(from.size * struct_words);

	for (u32 i = 0; i < from.size; ++i) {
		struct_expand((new_arr + i * struct_words), *this, p[i]);
	}

	return new_arr;
}*/


usize StringOffset::calc_extra_size(usize cur_size, Expander& expander, StringOffset const& src) {
	TL_UNUSED(expander); TL_UNUSED(src);
	return cur_size + round_size_up(src.size);
}

void StringOffset::expand_raw(Ref<StringOffset> dest, Expander& expander, StringOffset const& src) {
	TL_UNUSED(expander);

	dest.set(expander.alloc_str_raw(src.get()));
}


}