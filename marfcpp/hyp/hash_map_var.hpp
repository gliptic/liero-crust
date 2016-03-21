#ifndef HASH_MAP_VAR_HPP
#define HASH_MAP_VAR_HPP

#include <tl/cstdint.h>

struct hash_map_var {

	u8 state(u32 index) {
		return (bitmap[index >> 5] >> ((index & 31) << 1)) & 3;
	}

	void set_state(u32 index, u8 s) {
		u32 wordindex = index >> 5;
		int shift = ((index & 31) << 1);
		auto cur = bitmap[wordindex];
		cur &= ~(3 << shift);
		cur |= s << shift;
		bitmap[wordindex] = cur;
	}

	u64* bitmap;
	u8* data;
};

#endif // HASH_MAP_VAR_HPP
