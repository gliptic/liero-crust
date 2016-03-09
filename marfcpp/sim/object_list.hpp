#ifndef OBJECT_LIST_HPP
#define OBJECT_LIST_HPP

#include <cstddef>

template<typename T, int Limit>
struct fixed_object_list
{
	struct range
	{
		range(T* cur, T* end)
			: cur(cur), end(end)
		{
		}

		bool next(T*& ret)
		{
			T* r = cur;
			++cur;

			ret = r;

			return r != end;
		}

		T* cur;
		T* end;
	};

	fixed_object_list()
	{
		clear();
	}

	T* new_object_reuse()
	{
		T* ret;
		if (count == Limit)
			ret = &arr[Limit - 1];
		else
			ret = get_free_object();

		return ret;
	}

	T* new_object()
	{
		if (count == Limit)
			return 0;

		T* ret = get_free_object();
		return ret;
	}

	range all()
	{
		return range(arr, arr + count);
	}

	void free(T* ptr)
	{
		assert(ptr < &arr[0] + count && ptr >= &arr[0]);
		*ptr = arr[--count];
	}

	void free(range& r)
	{
		free(--r.cur);
		--r.end;
	}

	void clear()
	{
		count = 0;
	}

	std::size_t size() const
	{
		return count;
	}

private:
	T* get_free_object()
	{
		assert(count < Limit);
		T* ptr = &arr[count++];
		return ptr;
	}

	T arr[Limit];
	std::size_t count;
};

#endif // OBJECT_LIST_HPP
