#ifndef AST_HPP
#define AST_HPP

#include <tl/cstdint.h>
#include <tl/std.h>
#include <tl/vector.hpp>

#include "string_set.hpp"

namespace hyp {

enum node_type {
	NT_CALL = 0,
	NT_KI32 = 1,
	NT_KF64 = 2,
	NT_NAME = 3,
	NT_LAMBDA = 4,
	NT_UPVAL = 5,

	NT_MATCHUPV = 6,
	NT_MATCH = 7,
	
	NT_MAX
};

struct node {
	u32 a, b;

	static node make(u32 type, u32 val, u32 val2) {
		node n;
		assert(val < (1ull << 60));
		n.b = type | (val << 4);
		n.a = val2;
		return n;
	}

	static node make_str(strref ref) {
		node n;
		n.b = (ref.b << 4) | NT_NAME;
		n.a = ref.a;
		return n;
	}

	static node make_matchupv(node upv) {
		node n;
		assert(upv.type() == NT_UPVAL);
		n.b = (upv.b & ~0xf) | NT_MATCHUPV;
		n.a = upv.a;
		return n;
	}

	static node make_upval(u32 level, u32 index) {
		node n;
		assert(level < (1<<8) && index < (1<<20));
		n.b = (level << 4) | NT_UPVAL;
		n.a = index;
		return n;
	}

	static node make_ref(u32 type, u32 ref, u32 value2 = 0) {
		node n;
		assert((ref & 3) == 0 && ref < (1 << 30));
		n.b = (ref << 2) | type;
		n.a = value2;
		return n;
	}

	node_type type() { return (node_type)(this->b & 0xf); }
	strref str() { return strref::from_raw(((u64)(this->b >> 4) << 32) | this->a); }
	u32 ref() { return (this->b >> 4) << 2; }
	u32 value2() { return this->a; }

	u32 upv_level() { return this->b >> 4; }
	u32 upv_index() { return this->a; }
};

struct node_match {
	node match, value;
};

struct call_op {
	//u32 argc;
	u32 type;
	node fun;
	node args[];
};

struct lambda_op {
	//u16 argc;
	//u16 bindings;
	u32 data[];
};

struct local {
	strref name;
	node ref;
};

struct module {

	tl::mixed_buffer binding_arr, expr_arr;

	module() { }
	module(module&& other) = default;
	module& operator=(module&& other) = default;

};

TL_STATIC_ASSERT((sizeof(lambda_op) & 3) == 0);
TL_STATIC_ASSERT((sizeof(call_op) & 3) == 0);

static strref const str_empty(0);

inline strref str_short3(char a, char b, char c) {
	return strref::from_short((a << 16) | (b << 8) | c, 3);
}

inline strref str_short2(char a, char b) {
	return strref::from_short((a << 8) | b, 2);
}

inline strref str_short1(char a) {
	return strref::from_short(a, 1);
}

//static node const node_name_empty = { NT_NAME | (str_empty << 4) };
static node const node_none = { 0, 0 };

}

#endif // AST_HPP
