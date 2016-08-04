#ifndef AST_HPP
#define AST_HPP

#include <tl/cstdint.h>
#include <tl/std.h>
#include <tl/vec.hpp>

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

struct node : u64_align4 {

	node() {
	}

	explicit node(u64 v)
		: u64_align4(v) {
	}

	static node make_pair(u32 a, u32 b) {
		return node(((u64)a << 32) | b);
	}

	static node make(u32 type, u32 val, u32 val2) {
		assert(val < (1 << 28));
		return node((u64)(type | (val << 4)) | ((u64)val2 << 32));
	}

	static node make_str(strref ref) {
		return node((ref.raw() << 4) | NT_NAME);
	}

	static node make_matchupv(node upv) {
		assert(upv.type() == NT_UPVAL);
		return node((upv.get() & ~0xf) | NT_MATCHUPV);
	}

	static node make_upval(u32 level, u32 index) {
		assert(level < (1<<8) && index < (1<<20));
		return node((u64)((level << 4) | NT_UPVAL) | ((u64)index << 32));
	}

	static node make_ref(u32 type, u32 ref, u32 value2 = 0) {
		assert((ref & 3) == 0 && ref < (1 << 30));
		return node((u64)((ref << 2) | type) | ((u64)value2 << 32));
	}

	node_type type() { return (node_type)(this->get() & 0xf); }
	strref str() { return strref::from_raw(this->get() >> 4); }
	u32 ref() { return (((u32)this->get() >> 4) << 2); }
	u32 value1() { return (u32)this->get(); }
	u32 value2() { return (u32)(this->get() >> 32); }

	u32 upv_level() { return (u32)this->get() >> 4; }
	u32 upv_index() { return (u32)(this->get() >> 32); }
};

struct node_match {
	node match, value;
};

// Types

struct type_node {
	u32 v;

	explicit type_node(u32 v_init) : v(v_init) {}

	static type_node integer(bool is_signed, int bits) {
		assert(bits <= 64);
		return type_node((is_signed << 1) | (bits << 2));
	}

	static type_node sinteger(int bits) {
		return integer(true, bits);
	}
	
	bool operator==(type_node const& other) const {
		return this->v == other.v;
	}
};

static type_node const type_unit = type_node::sinteger(0);
static type_node const type_sint32 = type_node::sinteger(32);

struct typed_node {
	typed_node(node n, type_node tn)
		: n(n), tn(tn) {
	}

	node n;
	type_node tn;
};

// Ops

struct call_op {
	//u32 argc;
	//type_node type;
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

	//tl::mixed_buffer binding_arr, expr_arr;
	node root;

	module() { }
	module(module&& other) = default;
	module& operator=(module&& other) = default;

};

//TL_STATIC_ASSERT(sizeof(lambda_op) == 4);
TL_STATIC_ASSERT(sizeof(call_op) == 8);

TL_STATIC_ASSERT(sizeof(node) == 8);
TL_STATIC_ASSERT(sizeof(strref) == 8);
TL_STATIC_ASSERT(sizeof(local) == 16);

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
static node const node_none(0);

}

#endif // AST_HPP
