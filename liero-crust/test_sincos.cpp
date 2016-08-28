#include <tl/std.h>
#include <tl/approxmath/am.hpp>
#include <stdio.h>

using namespace tl;

struct SinCosGo {
	auto operator()(i32 x) {
		auto r = sincos(x * ((1.0 / 65536.0) * tl::pi2 / 128.0));

		return tl::VectorI2(i32(r.x * 65536.0), i32(r.y * 65536.0));
	}
};

struct SinCos1 {
	auto operator()(i32 x) {
		return sincos_fixed(x);
	}
};

struct SinCos2 {
	auto operator()(i32 x) {
		return sincos_fixed2(x);
	}
};

template<typename F>
inline void print_timer(char const* name, F f) {
	i32 sum = 0;

	u64 time = tl::timer([&]{
		i32 x = 100 << 16;
#ifdef NDEBUG
		for (int i = 0; i < 120000000; ++i) {
#else
		for (int i = 0; i < 12; ++i) {
#endif
			auto v = f(x);
			sum += v.x - v.y;
			x += 1 << 13;
		}
	});

	printf("%s: %f ms, %d\n", name, time / 10000.0, sum);
}

void test_sincos() {
	printf("Start test\n");
	print_timer("sincos_go", SinCosGo());
	print_timer("sincos_1", SinCos1());
	print_timer("sincos_2", SinCos2());

#if 0
	{
		i32 x = 100 << 16;
#ifdef NDEBUG
		for (int i = 0; i < 120000000; ++i) {
#else
		for (int i = 0; i < 12; ++i) {
#endif
			auto a = sincos_fixed(x);
			auto b = sincos_fixed2(x);
			
			if (a != b)
				printf("%d: %d %d, %d %d\n", x, a.x, a.y, b.x, b.y);

			x += 1 << 13;
		}
	}
#endif
}
