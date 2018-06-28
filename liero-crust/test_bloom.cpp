#include <tl/tree.hpp>
#include <tl/std.h>
#include <tl/vec.hpp>

struct NodeHeader {
	u32 count_keys;
	u32 count_insert_msg;

	NodeHeader()
		: count_keys(0)
		, count_insert_msg(0) {
	}
};

struct Node {
	static u32 const size = 4 * 1024 * 1024;
	u8 buf[size];

	typedef u64 Key;
	typedef u64 Value;

	struct InnerBranch {
		Key key;
		u64 heap_pos;
	};

	struct Leaf {
		Key key;
		Value value;

		Leaf(Key key, Value value)
			: key(key), value(value) {
		}
	};

	static_assert(sizeof(InnerBranch) == 16, "");

	Node() {
		new (header()) NodeHeader();
	}

	NodeHeader* header() {
		return (NodeHeader *)buf;
	}

	InnerBranch* first_key() {
		return (InnerBranch *)(buf + sizeof(NodeHeader));
	}

	InnerBranch* last_key() {
		auto* h = header();
		return (InnerBranch *)(buf + sizeof(NodeHeader)) + h->count_keys;
	}

	Leaf* first_insert_msg() {
		auto* h = header();
		return (Leaf *)(buf + size) - h->count_insert_msg;
	}

	tl::VecSlice<InnerBranch> keys() {
		auto f = first_key();
		return tl::VecSlice<InnerBranch>(f, f + count_keys());
	}

	u32 count_keys() {
		return header()->count_keys;
	}

	void insert(Leaf leaf) {
		auto* h = header();
		auto* next_first_leaf = first_insert_msg() - 1;
		if ((u8 *)last_key() >= (u8 *)next_first_leaf) {
			assert(!"not implemented");
		}

		*next_first_leaf = leaf;
		++h->count_insert_msg;
	}
};

void test_bloom() {
	Node* n = new Node();
	n->insert(Node::Leaf(0, 1));
	n->insert(Node::Leaf(0, 1));
	
	
}