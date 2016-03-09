#ifndef STRING_SET_HPP
#define STRING_SET_HPP

#include <tl/cstdint.h>
#include <tl/vector.hpp>

namespace hyp {

struct string_set {

	struct slot {
		u32 h, i;
	};

	u32 hshift, keycount;
	slot* tab;
	tl::vector<u8> buf;

	string_set();
	~string_set();
	bool get(u32 hash, u8 const* str, u32 len, u32* ret);
	u32 count();
};

}

#endif // STRING_SET_HPP

