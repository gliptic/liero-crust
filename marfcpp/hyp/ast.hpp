#ifndef AST_HPP
#define AST_HPP

#include <tl/cstdint.h>
#include <tl/std.h>
#include <tl/vector.hpp>

namespace hyp {

static int const small_int_bits = 32 - 4;
static int const small_int_lim = 1 << (small_int_bits - 1);

enum {
	NT_CALL = 0,
	NT_KI28 = 1,
	NT_KI32 = 2,
	NT_KF64 = 3,
	NT_NAME = 4,
	NT_LAMBDA = 5,
	NT_UPVAL = 6,

	NT_MATCHUPV = 7,
	NT_MATCH = 8,

	NT_MAX
};

struct node {
	u32 v;

	static node make(u32 type, u32 val) {
		node n;
		assert(val < (1 << 28));
		n.v = type | (val << 4);
		return n;
	}

	static node make_str(u32 ref) {
		node n;
		assert(ref < (1 << 30));
		n.v = (ref << 4) | NT_NAME;
		return n;
	}

	static node make_matchupv(node upv) {
		node n;
		assert((upv.v & 0xf) == NT_UPVAL);
		n.v = (upv.v & ~0xf) | NT_MATCHUPV;
		return n;
	}

	static node make_upval(u32 level, u32 index) {
		node n;
		assert(level < (1<<8) && index < (1<<20));
		n.v = (index << 12) | (level << 4) | NT_UPVAL;
		return n;
	}

	static node make_ref(u32 type, u32 ref) {
		node n;
		assert((ref & 3) == 0 && ref < (1 << 30));
		n.v = (ref << 2) | type;
		return n;
	}

	u32 type() { return this->v & 0xf; }
	u32 str() { return this->v >> 4; }
	u32 to_ref() { return (this->v >> 4) << 2; }

	u32 upv_level() { return (this->v >> 4) & 0xff; }
	u32 upv_index() { return (this->v >> 12) & 0xfffff; }
};

struct node_match {
	node match, value;
};

struct base_op {
	u32 type, argc;
};

struct op : base_op {

};

struct call_op : base_op {
	node fun;
	node args[];
};

struct lambda_op : base_op {
	u32 bindings;
	u8 data[];
};

struct local {
	u32 name;
	node ref;
};

struct module {

	tl::vector<u8> binding_arr, expr_arr;

	module() { }
	module(module&& other) = default;
	module& operator=(module&& other) = default;

};

TL_STATIC_ASSERT((sizeof(lambda_op) & 3) == 0);
TL_STATIC_ASSERT((sizeof(call_op) & 3) == 0);

static u32 const str_empty = 0;

inline u32 str_short3(char a, char b, char c) {
	return (u32)((3 << 24) | (a << 16) | (b << 8) | c);
}

inline u32 str_short2(char a, char b) {
	return (u32)((2 << 24) | (a << 8) | b);
}

inline u32 str_short1(char a) {
	return (u32)((1 << 24) | a);
}

static node const node_name_empty = { NT_NAME | (str_empty << 4) };
static node const node_none = { 0 };

}

#endif // AST_HPP
