#include <tl/rand.hpp>
#include <stdio.h>

template<typename F, typename T>
inline void print_timer(char const* name, T limits[32], F f) {
	i32 sum = 0;

	u64 time = tl::timer([&] {
#ifdef NDEBUG
		for (int i = 0; i < 220000000; ++i) {
#else
		for (int i = 0; i < 12; ++i) {
#endif
			i32 v = f(limits[i & 31]);
			sum += v;
		}
	});

	printf("%s: %f ms, %d\n", name, time / 10000.0, sum);
}

void test_rand() {
	tl::LcgPair r;

	i32 limits[32];
	f64 flimits[32];

	for (int i = 0; i < 32; ++i) {
		limits[i] = 100 + r.get_i32_fast_(50000);
		flimits[i] = limits[i] / 4294967296.0;
	}

	print_timer("mul shift", limits, [&](i32 lim) { return r.get_i32_fast_(lim); });
	print_timer("float mul", flimits, [&](f64 lim) { return r.get_i32(lim); });
}