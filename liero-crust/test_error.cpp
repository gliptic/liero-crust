#include <tl/std.h>
#include <tl/result.hpp>
#include <tl/vec.hpp>
#include <stdio.h>

void test_error() {
	using namespace tl;

	tl::Vec<u8> vec;
	vec.push_back(1);
	auto x = Result<tl::Vec<u8>, u8>::ok(move(vec))
		.map([](auto x) { x.push_back(2); return move(x); })
		.unwrap();

	printf("%d\n", x.size());
}
