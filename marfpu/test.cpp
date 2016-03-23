#include "tl/std.h"
#include <utility>
#include <stdio.h>

using std::move;

#define TL_TRIVIAL_MOVE(name) \
	name(name const&) = delete; \
	name(name&&) = default; \
	name& operator=(name const&) = delete;

struct A {
	int* x;

	A() = delete;
	TL_TRIVIAL_MOVE(A)

	A(int* x) {
		this->x = x;
		++*this->x;
	}

	~A() {
		if (x) --*x;
		x = 0;
	}
};

extern "C" void testcpp() {
	int i = 0;
	A a(&i);
	A* pa = new A(&i);

	A b(move(a));

	printf("%d\n", *b.x);
}