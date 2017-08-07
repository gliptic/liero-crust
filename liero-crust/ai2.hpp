#ifndef LIERO_AI2_HPP
#define LIERO_AI2_HPP

#include "lierosim/state.hpp"

//#include "tl/tree.hpp"

namespace liero {
namespace ai {

struct ChildRef {
	u32 step;
};

struct Node {
	Node(ModRef mod) {

	}

	tl::Vec<WormInput> inputs;
	u32 saved_state_id;
	
};

}
}

#endif
