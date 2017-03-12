#ifndef LIERO_OBJECT_LIST_HPP
#define LIERO_OBJECT_LIST_HPP 1

#include <tl/std.h>
#include <tl/vec.hpp>

namespace liero {

template<int Limit, bool QueueNew>
struct FixedObjectListBase {
	usize count;

	FixedObjectListBase& operator=(FixedObjectListBase const& other) = default;

	bool is_full() {
		return count == Limit;
	}

	usize next_free_index() {
		assert(!is_full());
		return count++;
	}

	usize get_overwrite_index() {
		assert(is_full());
		return Limit - 1;
	}

	usize next_free_overwrite_index() {
		auto index = tl::min(count, usize(Limit) - 1);
		this->count = index + 1;
		return index;
	}

	void clear() {
		this->count = 0;
	}
};

template<int Limit>
struct FixedObjectListBase<Limit, true> {
	usize count;
	usize old_count, new_count;

	FixedObjectListBase& operator=(FixedObjectListBase const& other) = default;

	bool is_full() {
		return new_count >= Limit;
	}

	usize next_free_index() {
		assert(!is_full());
		return new_count++;
	}

	usize get_overwrite_index() {
		assert(is_full());

		if (this->count < this->old_count) {
			// Room left in removed range
			return --this->old_count;
		} else {
			new_count = Limit + 1;
			return Limit;
		}
	}

	void clear() {
		this->count = 0;
		this->old_count = this->new_count = 0;
	}

};

template<typename T, int Limit, bool QueueNew = false>
struct FixedObjectList : FixedObjectListBase<Limit, QueueNew> {

	struct Range {
		Range(T* cur, T* end)
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
		this->clear();
	}

	FixedObjectList(FixedObjectList const&) = delete;

	FixedObjectList& operator=(FixedObjectList const& other) {
		FixedObjectListBase<Limit, QueueNew>::operator=(other);
		memcpy(arr, other.arr, other.count * sizeof(T));
		return *this;
	}
	
	T* get_free_object() {
		
		T* ptr = &arr[this->next_free_index()];
		//new (ptr, tl::non_null()) T();
		return ptr;
	}

	T* new_object_reuse() {
		return &arr[this->next_free_overwrite_index()];
	}
	
	template<typename DestroyFunc>
	T* new_object_reuse(DestroyFunc destroy_func) {
		T* ret;
		if (is_full()) {
			usize replace_idx = this->get_overwrite_index();
			ret = &arr[replace_idx];

			if (!QueueNew) {
				destroy_func(ret, replace_idx);
			}
		} else {
			ret = get_free_object();
		}

		return ret;
	}

	T* new_object_reuse_queue() {
		usize idx;
		if (count != this->old_count) {
			idx = --this->old_count;
		} else if (this->new_count < Limit) {
			idx = this->new_count++;
		} else {
			idx = Limit;
			this->new_count = Limit + 1;
		}

		return &arr[idx];
	}
	
	T* new_object() {
		if (is_full())
			return 0;
			
		T* ret = get_free_object();
		return ret;
	}

	Range all() {
		return Range(arr, arr + this->count);
	}
	
	void free(T* ptr) {
		assert(ptr < &arr[0] + this->count && ptr >= &arr[0]);
		*ptr = arr[--this->count];
	}
	
	void free(Range& r)	{
		free(--r.cur);
		--r.end;
	}

	usize size() const {
		return this->count;
	}

	u32 index_of(T const* v) const {
		return u32(v - arr);
	}

	T& of_index(u32 idx) {
		assert(idx < this->count);
		return arr[idx];
	}

	// Only valid if QueueNew == true
	template<typename DestroyFunc>
	tl::VecSlice<T> flush_new_queue(DestroyFunc destroy_func) {
		static_assert(QueueNew, "Must have queue enabled to use it");

		if (this->new_count > Limit) {
#if 1
			if (this->count == Limit) {
				destroy_func(&arr[Limit - 1], Limit - 1);
				this->count = Limit - 1;
			}

			if (old_count == Limit) {
				old_count = Limit - 1;
			}

			arr[Limit - 1] = arr[Limit];
#endif

			new_count = Limit;
		}

#if !QUEUE_REMOVE_NOBJS
		if (this->count != this->old_count) {
			// We need to move objects
			assert(this->count < this->old_count);

			// The new objects are rotated down into the gap. This minimizes the amount of data
			// that needs to move.
			usize added = this->new_count - this->old_count;
			usize gap_to_fill = tl::min(this->old_count - this->count, added);

			memcpy(arr + this->count, arr + (this->new_count - gap_to_fill), sizeof(T) * gap_to_fill);

			this->new_count = this->count + added;
		}
#else
		assert(this->count == this->old_count);
#endif

		tl::VecSlice<T> ret(arr + count, arr + new_count);
		this->count = this->new_count;
		this->old_count = this->new_count;
		return ret;
	}

	void reset_new_queue() {
		this->new_count = this->old_count = this->count;
	}

	T arr[Limit + 1];
};

}

#endif // LIERO_OBJECT_LIST_HPP
