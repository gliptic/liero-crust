#ifndef LOCAL_SET_HPP
#define LOCAL_SET_HPP 1

#include <tl/std.h>

namespace hyp {

struct local_set {
	struct slot {
		u64 key;
		u64 value;
	};

	local_set();
	u64* get(u64 key);
	u64 remove(u64 key);
	u32 max_dtb();
	double avg_dtb();

	u32 hshift, keycount;
	slot* tab;
};

}

#endif // LOCAL_SET_HPP
