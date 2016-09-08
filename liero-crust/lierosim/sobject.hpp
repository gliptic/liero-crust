#ifndef LIERO_SOBJECT_HPP
#define LIERO_SOBJECT_HPP

#include <tl/vector.hpp>

namespace liero {

struct SObjectType;
struct State;
struct TransientState;

void create(SObjectType const& self, State& state, TransientState& transient_state, tl::VectorI2 pos);

struct SObject {
	tl::VectorI2 pos;
	u32 ty_idx;
	u32 time_to_die;

};

}

#endif // LIERO_SOBJECT_HPP
