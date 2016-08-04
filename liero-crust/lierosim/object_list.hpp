#ifndef LIERO_OBJECT_LIST_HPP
#define LIERO_OBJECT_LIST_HPP 1

#include <tl/std.h>

namespace liero {

template<typename T, int Limit>
struct FixedObjectList
{
	struct range {
		range(T* cur, T* end)
			: cur(cur), end(end) {
		}

		T* next() {
			T* ret = cur;
			++cur;

			return ret == end ? 0 : ret;
		}

		T* cur;
		T* end;
	};

	FixedObjectList() {
		clear();
	}
	
	T* get_free_object() {
		assert(count < Limit);
		T* ptr = &arr[count++];
		return ptr;
	}
	
	T* new_object_reuse() {
		T* ret;
		if (count == Limit)
			ret = &arr[Limit - 1];
		else
			ret = get_free_object();

		return ret;
	}
	
	T* new_object() {
		if (count == Limit)
			return 0;
			
		T* ret = get_free_object();
		return ret;
	}

	range all() {
		return range(arr, arr + count);
	}
	
	void free(T* ptr) {
		assert(ptr < &arr[0] + count && ptr >= &arr[0]);
		*ptr = arr[--count];
	}
	
	void free(range& r)	{
		free(--r.cur);
		--r.end;
	}
	
	void clear() {
		count = 0;
	}
	
	usize size() const {
		return count;
	}

	u32 index_of(T const* v) const {
		return u32(v - arr);
	}

	T& of_index(u32 idx) {
		assert(idx < count);
		return arr[idx];
	}
	
	T arr[Limit];
	usize count;
};

}

#endif // LIERO_OBJECT_LIST_HPP
