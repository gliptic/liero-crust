#ifndef LIERO_AI2_HPP
#define LIERO_AI2_HPP

#include <cmath>
#include "lierosim/state.hpp"
//#include <tl/hash_map.hpp>

namespace liero {
namespace ai {

using std::unique_ptr;

struct Node;

struct ChildRef {
	ChildRef(Node* node_new)
		: node(node_new) {
	}

	unique_ptr<Node> node;
};

struct NodeChildren {
	NodeChildren() : n(0) {
	}

	u32 n; // Times visited
	tl::Vec<ChildRef> children;
};

struct MeanVar {
	
	MeanVar() : mean(0.0), m2(0.0) {
	}

	// n = 1, 2, ...
	void add(f64 x, i32 n) {
		auto d = x - this->mean;
		this->mean += d / n;
		auto d2 = x - this->mean;
		this->m2 += d * d2;
	}

	f64 var(i32 n) const {
		return n < 2 ? 0.0 : this->m2 / (n - 1);
	}

	f64 mean, m2;
};

struct WormInputs {
	WormInputs(WormInput own, WormInput other)
		: own(own), other(other) {
	}

	WormInput own, other;
};

struct Node : NodeChildren {
	Node() : first(0), deviation(0) {

	}

	u32 deviation;

	MeanVar stats;

	f64 b(f64 e) const {
		return stats.mean + std::sqrt(2 * e * stats.var(this->n) / this->n) + 3 * e / this->n;
	}

	u32 first;
	tl::Vec<WormInputs> inputs;
	unique_ptr<State> tail_state;
};

struct Ai2 {
	tl::Vec<std::unique_ptr<State>> unused_states;
	ModRef mod_ref;
	TransientState spare_transient_state;
	State spare_state;

	tl::LcgPair rand;
	unique_ptr<Node> root;

	Ai2(ModRef mod)
		: mod_ref(mod)
		, rand(1, 1)
		, root(new Node)
		, spare_state(mod) {
	}

	void do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state);
	void done_frame(u32 worm_index, TransientState& transient_state);
};

}

}

#endif
