#include <tl/std.h>

struct Struct {

	template<typename T, usize Offset>
	TL_FORCE_INLINE T& _field() {
		return *reinterpret_cast<T *>(reinterpret_cast<u8 *>(this) + Offset);
	}

	template<typename T, usize Offset>
	TL_FORCE_INLINE T const& _field() const {
		return *reinterpret_cast<T const*>(reinterpret_cast<u8 const *>(this) + Offset);
	}
};

struct ThingReader {
	u8 data[16];

};

struct Thing {
	u8 data[16];


};

void test_ser() {
}