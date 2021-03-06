#include "../io.hpp"
#include "../std.h"

//#include "miniwindows.h"
#include "win.hpp"

namespace tl {

void sprint(tl::StringSlice str) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dummy;
	WriteConsoleA(
		h,
		str.begin(),
		(DWORD)str.size(),
		&dummy,
		NULL);
}

void sprint(char const* s) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dummy;
	WriteConsoleA(
		h,
		s,
		(DWORD)strlen(s),
		&dummy,
		NULL);
}

static char* utos(uint32_t v, char* ends) {
	*--ends = 0;
	do {
		*--ends = '0' + (v % 10);
		v /= 10;
	} while (v);
	return ends;
}

void uprint(uint32_t v) {
	char buf[10];
	char *b = utos(v, buf + 10);
	sprint(b);
}

void iprint(int32_t v) {
	char buf[11], isneg = 0;

	if (v < 0) {
		v = -v;
		isneg = 1;
	}

	char *b = utos((uint32_t)v, buf + 11);
	if (isneg) *--b = '-';
	sprint(b);
}

void fprint(double v) {
	char buf[64];

	wsprintfA(buf, "%f", v);
	sprint(buf);
}

}
