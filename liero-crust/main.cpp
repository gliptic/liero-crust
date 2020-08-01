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
void test_net();
void test_net2();
void test_net3();
void test_net4();
void test_net5();
void test_error();
void test_rand();
void test_ser();
void test_ser2();
void test_irc();
void test_http();
void test_bloom();
void test_lmdb();
void test_statepack();

namespace test_cellphase {
void test_cellphase();
}

namespace test_ships {
void test_ships();
}

int main() {

	//test_gfx();
	//test_sincos();
	//test_gui();

	//test_net();
	//test_net2();
	//test_net3();
	//test_net4();
	//test_net5();
	//test_io();
	//test_rand();
	//test_ser();
	//test_ser2();

	//test_irc();
	//test_http();

	//test_error();
	//test_bloom();
	//test_lmdb();
	//test_cellphase::test_cellphase();
	//test_ships::test_ships();
	test_statepack();

	return 0;
}
