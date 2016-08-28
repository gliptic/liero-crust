#ifndef BASE_MODEL_HPP
#define BASE_MODEL_HPP 1

#include <tl/string.hpp>
#include <tl/bits.h>
#include "config.hpp"

namespace ss {

struct alignas(8) Offset {
	i32 offs;
	u32 size;

	u8* ptr() { return ptr_rel(this); }
	u8 const* ptr() const { return ptr_rel(this); }

	u8* ptr_rel(Offset* other) {
		return (u8*)(other + 1) + (offs << 3);
	}

	u8 const* ptr_rel(Offset const* other) const {
		return (u8*)(other + 1) + (offs << 3);
	}

	void ptr_default() {
		this->offs = 0;
		this->size = 0;
	}

	void ptr(void const* p, u32 size_new) {
		isize offs_b = (u8 const*)p - (u8 const*)(this + 1);
		assert((offs_b & 7) == 0);
		this->offs = tl::narrow<i32>(offs_b >> 3);
		this->size = size_new;
	}

	void ptr(void const* p) {
		isize offs_b = (u8 const*)p - (u8 const*)(this + 1);
		assert((offs_b & 7) == 0);
		this->offs = tl::narrow<i32>(offs_b >> 3);
	}
};

template<typename T>
struct StructRef : Offset {
	T* get() {
		return (T *)this->ptr();
	}

	T* get_rel(Offset* other) {
		return (T *)this->ptr_rel(other);
	}

	T const* get() const {
		return (T const*)this->ptr();
	}

	T const* get_rel(Offset* other) const {
		return (T const*)this->ptr_rel(other);
	}

	void set(T const* p) {
		this->ptr(p, sizeof(T) >> 3);
	}
};

template<typename T>
struct ArrayRef : Offset {
	T* get() {
		return (T *)this->ptr();
	}

	T* get_rel(Offset* other) {
		return (T *)this->ptr_rel(other);
	}

	T const* get() const {
		return (T const*)this->ptr();
	}

	T const* get_rel(Offset* other) const {
		return (T const*)this->ptr_rel(other);
	}

	void set(T const* p, u32 size_new) {
		this->ptr(p, size_new);
	}

	void set_empty() {
		this->ptr_default();
	}
};

struct StringRef : Offset {

	tl::StringSlice get() const {
		u8 const* p = this->ptr();
		return tl::StringSlice(p, p + this->size);
	}

	tl::StringSlice get_rel(Offset const* other) const {
		u8 const* p = this->ptr_rel(other);
		return tl::StringSlice(p, p + this->size);
	}

	void set(tl::StringSlice str) {
		this->ptr(str.begin(), tl::narrow<u32>(str.size()));
	}
};

struct Writer;

template<typename T>
struct WriterRef {
	Writer* writer;
	u32 offs;

	WriterRef(Writer& writer_init, u32 offs_init)
		: writer(&writer_init), offs(offs_init) {
	}

	T& operator*();
	T* operator->();
	T& operator[](usize i);

	WriterRef operator+(u32 offs);
};

struct WriterStringRef {
	Writer* writer;
	u32 offs, size;

	WriterStringRef(Writer& writer_init, u32 offs_init, u32 size_init)
		: writer(&writer_init), offs(offs_init), size(size_init) {
	}

	inline tl::StringSlice operator*();
};

struct Writer {
	tl::BufferMixed buf;

	Writer() {
	}

	// TODO: This allocation should zero the last word
	// so no random data leaks when allocating arrays of uneven size.

	template<typename T>
	WriterRef<T> alloc(usize count = 1) {
		usize size = sizeof(T) * count;
		if ((sizeof(T) & 7) != 0) {
			size = ((size - 1) | 7) + 1;
			// TODO: Clear last word if non-zero allocation
		}
		u8* ptr = this->buf.unsafe_alloc(size);
		return WriterRef<T>(*this, tl::narrow<u32>(ptr - this->buf.begin()));
	}

	WriterStringRef alloc_str(tl::StringSlice str) {
		usize str_size = str.size();
		usize size = ((str_size - 1) | 7) + 1;
		u8* ptr = this->buf.unsafe_alloc(size);
		memcpy(ptr, str.begin(), str_size);
		
		return WriterStringRef(*this,
			tl::narrow<u32>(ptr - this->buf.begin()),
			tl::narrow<u32>(str_size));
	}

	tl::BufferMixed to_buffer() {
		return std::move(this->buf);
	}
};

template<typename T>
T& WriterRef<T>::operator*() {
	return *(T *)(writer->buf.begin() + offs);
}

template<typename T>
T* WriterRef<T>::operator->() {
	return (T *)(writer->buf.begin() + offs);
}

template<typename T>
T& WriterRef<T>::operator[](usize i) {
	return *((T *)(writer->buf.begin() + offs) + i);
}

template<typename T>
WriterRef<T> WriterRef<T>::operator+(u32 i) {
	return WriterRef<T>(*writer, this->offs + sizeof(T) * i);
}

inline tl::StringSlice WriterStringRef::operator*() {
	u8* p = writer->buf.begin() + offs;
	return tl::StringSlice(p, p + size);
}

struct Expander : Writer {
	Expander(tl::VecSlice<u8 const> data)
		: data(data) {
	}

	template<typename To, typename From>
	WriterRef<To> expand_array(Offset const& from) {
		auto p = (From const *)from.ptr();

		WriterRef<To> new_arr = this->alloc<To>(from.size);

		for (u32 i = 0; i < from.size; ++i) {
			To::expand((new_arr + i), *this, p[i]);
		}

		return new_arr;
	}

	tl::VecSlice<u8 const> data;
};

}

#endif
