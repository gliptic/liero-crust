#ifndef SMALL_VEC_HPP
#define SMALL_VEC_HPP

#include <tl/vector.hpp>

namespace hyp {

template<typename T, usize Size>
struct small_vector : tl::vector<T> {
	small_vector() {
		this->v.
	}

	u64 inline_data[(Size + sizeof(u64) - 1) / sizeof(u64)];
};

}

#endif // SMALL_VEC_HPP
