#include <stdlib.h>
#include <emmintrin.h>
#include <tl/std.h>
#include <tl/bits.h>
#include <stdio.h>

void test_gfx();
void test_comp();
void test_sincos();
void test_gui();
void test_io();

int main() {

	//test_sincos();
	//test_gui();

	test_gfx();
	//test_io();

	return 0;
}
