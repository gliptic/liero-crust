#include <tl/vec.hpp>
#include <tl/vector_old.hpp>
#include <tl/vector.hpp>

namespace vectest {

using tl::vector;
using tl::VectorD2;

void check_(bool cond, char const* msg, char const* ctx) {
	if (!cond) {
		printf("[%s] Condition failed: %s\n", ctx, msg);
	}
}

#define check(cond) check_((cond), #cond, Ctx)
#define checkm(cond, msg) check_(cond, #cond " - " msg, Ctx)

#define doboth(a) { vec a; vec2 a; }
#define checkbetween(c) check((vec c) == (vec2 c))
#define checkboth(c) { check(vec c); check(vec2 c); }

template<typename VecT, typename Vec2T>
TL_NEVER_INLINE void test_vec(char const* Ctx) {
	VecT vec;
	Vec2T vec2;

	srand(1);
	int n = 3 + rand();

	doboth(.reserve(5));

	for (int i = 0; i < n; ++i) {
		doboth(.push_back(VectorD2(i, i)));
	}

	checkbetween([2].x);
	checkboth([2].y == 2.0);
	checkboth(.slice()[2].y == 2.0);
}


TL_NEVER_INLINE void test() {
	test_vec<vector<VectorD2>, tl::Vec<VectorD2>>("vector vs Vec");
	test_vec<vector<VectorD2>, tl::VecSmall<VectorD2>>("vector vs VecSmall");
}

}