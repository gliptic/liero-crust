#ifndef LIERO_SOBJECT_HPP
#define LIERO_SOBJECT_HPP

#include <tl/vector.hpp>

namespace liero {

struct SObject {
	tl::VectorI2 pos;
	u32 ty_idx;
	u32 time_to_die;

};

}

#endif // LIERO_SOBJECT_HPP
