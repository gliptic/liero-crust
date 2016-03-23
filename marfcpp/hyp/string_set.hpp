#ifndef STRING_SET_HPP
#define STRING_SET_HPP

#include <tl/cstdint.h>
#include <tl/vector.hpp>
#include <tl/std.h>

#include <stdio.h>

namespace hyp {

struct u64_align4 {
	//u32 a, b;
	u32 storage[2];

	u64_align4() {
	}

	u64_align4(u64 v) 
		/*: a((u32)v), b((u32)(v >> 32))*/ {

		memcpy(storage, &v, sizeof(u64));
	}

	u64 get() const {
		u64 v;
		memcpy(&v, storage, sizeof(u64));

		//return ((u64)this->b << 32) | (u64)this->a;
		return v;
	}

	bool operator==(u64_align4 const& other) const {
		return this->storage[0] == other.storage[0] && this->storage[1] == other.storage[1];
	}
};

struct strref : u64_align4 {

	strref(u64 v) 
		: u64_align4(v) {
	}

	u64 raw() const {
		return this->get();
	}

	void print(u8 const* base) const {
		u64 v = this->get();

		if (v >= (1ull << 59)) {
			u32 len = (v >> offset_bits) & ((1 << len_bits) - 1);
			u32 offset = v & ((1 << offset_bits) - 1);

			for (u32 i = 0; i < len; ++i) {
				printf("%c", (int)base[offset + i]);
			}

		} else {
			u32 len = (v >> 56) & 7;
			u64 chars = v & ((1ull << 56) - 1);

			u32 i = 0;
			for (; i < 7 && !(chars >> 56); ++i) {
				chars <<= 8;
			}

			for (; i < 7; ++i) {
				chars <<= 8;
				printf("%c", (int)(chars >> 56));
			}
		}
	}

	static strref from_raw(u64 v) {
		assert(v < (1ull << 60));
		return strref(v);
	}

	static strref from_offset(u32 offset, u32 len) {
		// 0xxx a*8 b*8 c*8 d*8 e*8 f*8 g*8
		// 1 len*29 offset*30

		assert(len < (1 << len_bits));
		//assert((offset & 3) == 0);
		assert(offset < (1 << offset_bits));

		return strref((u64)offset | ((u64)len << offset_bits) | (1ull << 59));
	}

	static strref from_short(u64 chars, u32 len) {
		assert(len <= 7);
		assert(chars < (1ull << 56));

		return strref(chars | (1ull << (len * 8)));
	}

private:
	static int const offset_bits = 30;
	static int const len_bits = 29;
};

struct string_set {

	struct slot {
		u32 h, i;
	};

	u32 hshift, keycount;
	slot* tab;
	tl::vector_slice<u8> source;

	string_set();
	~string_set();
	bool get(u32 hash, u8 const* str, u32 len, u32* ret);
	u32 count();
};

}

#endif // STRING_SET_HPP

