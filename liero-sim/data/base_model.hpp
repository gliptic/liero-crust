#ifndef BASE_MODEL_HPP
#define BASE_MODEL_HPP 1

#include <tl/platform.h>
#include <tl/string.hpp>
#include <tl/bits.h>
#include <type_traits>
#include "../config.hpp"

namespace ss {

struct Expander;

struct BaseRef {
	u8* ptr;
	u32 abs_offs;

	BaseRef(u8* ptr_init, u32 abs_offs)
		: ptr(ptr_init), abs_offs(abs_offs) {
	}

	TL_FORCE_INLINE BaseRef add_words(usize i) {
		return BaseRef(this->ptr + i * 8, this->abs_offs + i);
	}
};

template<typename T>
struct Ref : BaseRef {

	Ref(Ref const&) = default;
	Ref& operator=(Ref const&) = default;

	Ref(u8* ptr_init, u32 abs_offs)
		: BaseRef(ptr_init, abs_offs) {
	}

	Ref(Ref&& other)
		: BaseRef(other.ptr, other.abs_offs) {

		other.abs_offs = 0;
		other.ptr = 0;
	}

	template<typename U>
	void set(U other_ref) {
		((T *)ptr)->set_ref(*this, other_ref);
	}

	void set_raw(T other) {
		*(T *)ptr = other;
	}

	template<typename U, usize Offset>
	TL_FORCE_INLINE U& _field() {
		return *reinterpret_cast<U *>(ptr + Offset);
	}

#if SS_BACKWARD
	TL_FORCE_INLINE template<typename U, usize Offset>
	Ref<U> _field_ref() {
		return Ref<U>(ptr + Offset, abs_offs - u32(Offset / 8));
	}
#else
	template<typename U, usize Offset>
	TL_FORCE_INLINE Ref<U> _field_ref() {
		return Ref<U>(ptr + Offset, abs_offs + u32(Offset / 8));
	}

	TL_FORCE_INLINE Ref<T> operator+(usize i) {
		return Ref<T>(this->ptr + i * sizeof(T), this->abs_offs + tl::narrow<u32>(i * (sizeof(T) / 8)));
	}
#endif
};

struct StringRef : Ref<u8> {
	u32 size;

	StringRef(u8* ptr_init, u32 abs_offs, u32 size_init)
		: Ref(ptr_init, abs_offs), size(size_init) {
	}
};

template<typename T>
struct ArrayRef : Ref<T> {
	u32 size;

	ArrayRef(Ref<T> base, u32 size_init)
		: Ref(base), size(size_init) {
	}

#if SS_BACKWARD
	TL_FORCE_INLINE Ref<T> operator[](usize i) {
		return Ref<T>(this->ptr + i * sizeof(T), this->abs_offs - tl::narrow<u32>(i * (sizeof(T) / 8)));
	}
#else
	TL_FORCE_INLINE Ref<T> operator[](usize i) {
		return this->operator+(i);
	}
#endif
};

inline usize round_size_up(usize s) {
	return (s + 7) & ~usize(7);
}

struct Offset {
	u32 rel_offs;
	u32 size;

	u8* ptr() {
		return (u8 *)this + rel_offs * 8;
	}

	u8 const* ptr() const {
		return (u8 const *)this + rel_offs * 8;
	}

	void set(u32 rel_offs_new, u32 size_new) {
		this->rel_offs = rel_offs_new;
		this->size = size_new;
	}

	void set(u8 const* p, u32 size_new) {
		this->rel_offs = tl::narrow<u32>((p - (u8 *)this) >> 3); // TODO: Check
		this->size = size_new;
	}
};

struct StringOffset : Offset {
	using Offset::set;

	tl::StringSlice get() const {
		u8 const* p = ptr();
		return tl::StringSlice(p, p + this->size);
	}

	static usize calc_extra_size(usize cur_size, Expander& expander, StringOffset const& src);
	static void expand_raw(Ref<StringOffset> dest, Expander& expander, StringOffset const& src);

#if SS_BACKWARD
	void set_ref(Ref<StringOffset> self_ref, StringRef value) {
		this->set(self_ref.abs_offs - value.abs_offs, value.size);
	}
#else
	void set_ref(Ref<StringOffset> self_ref, StringRef value) {
		this->set(value.abs_offs - self_ref.abs_offs, value.size);
	}
#endif

#if 0
	void set(WriterStringRef ref) {
		auto sl = *ref;
		
		this->set(sl.begin(), ref.size);
	}
#endif
};

template<typename T>
struct ArrayOffset : Offset {
	using Offset::set;

	tl::VecSlice<T const> get() const {
		u8 const* p = ptr();
		return tl::VecSlice<T const>((T const *)p, (T const *)p + this->size);
	}

#if SS_BACKWARD
	void set_ref(Ref<ArrayOffset> self_ref, ArrayRef<T> value) {
		this->set(self_ref.abs_offs - value.abs_offs, value.size);
	}
#else
	void set_ref(Ref<ArrayOffset> self_ref, ArrayRef<T> value) {
		this->set(value.abs_offs - self_ref.abs_offs, value.size);
	}
#endif
};

template<typename T>
struct StructOffset : Offset {
	using Offset::set;

	T* get() {
		return (T *)ptr();
	}

	T const* get() const {
		return (T const *)ptr();
	}

#if SS_BACKWARD
	void set_ref(Ref<StructOffset<T>> self_ref, Ref<T> value) {
		// TODO: sizeof(T) is only valid if the struct storage isn't truncated
		this->set(self_ref.abs_offs - value.abs_offs, sizeof(T) / 8);
	}
#else
	void set_ref(Ref<StructOffset<T>> self_ref, Ref<T> value) {
		// TODO: sizeof(T) is only valid if the struct storage isn't truncated
		this->set(value.abs_offs - self_ref.abs_offs, sizeof(T) / 8);
	}
#endif
};

struct Struct {

	template<typename T, usize Offset>
	TL_FORCE_INLINE T& _field() {
		return *reinterpret_cast<T *>(reinterpret_cast<u8 *>(this) + Offset);
	}

	template<typename T, usize Offset>
	TL_FORCE_INLINE T const& _field() const {
		return *reinterpret_cast<T const*>(reinterpret_cast<u8 const *>(this) + Offset);
	}
};

struct Buffer {
	u8 *begin, *end, *cap;

	Buffer() : begin(0), end(0), cap(0) {
	}

#if SS_BACKWARD
	Buffer(u8* begin, u8* end) : begin(begin), end(end), cap(end) {
	}

	usize left() const {
		return end - begin;
	}
#else
	Buffer(u8* begin, u8* end) : begin(begin), end(begin), cap(end) {
	}

	usize left() const {
		return cap - end;
	}
#endif
	
};

struct Builder {
	static usize const BufSizeShift = 12;
	static usize const BufSizeMultiple = 1 << BufSizeShift;

	tl::Vec<Buffer> buffers;
	usize cur_abs_offs;
	Buffer* cur_buf;

	Builder()
		: cur_abs_offs(0), cur_buf(0) {
	}

#if SS_BACKWARD
	Ref<u8> alloc_words(usize words) {
		usize bytes = words * 8;

		if (!cur_buf || cur_buf->size() < bytes) {
			usize new_buf_size = ((bytes + BufSizeMultiple - 1) >> BufSizeShift) << BufSizeShift;
			u8* new_buf = (u8 *)malloc(new_buf_size);

			buffers.push_back(Buffer(new_buf, new_buf + new_buf_size));
			cur_buf = &buffers.back();
		}

		cur_buf->end -= bytes;
		cur_abs_offs += words;
		return Ref<u8>(cur_buf->end, tl::narrow<u32>(cur_abs_offs));
	}
#else
	Ref<u8> alloc_words(usize words) {
		usize bytes = words * 8;

		if (!cur_buf || cur_buf->left() < bytes) {
			usize new_buf_size = ((bytes + BufSizeMultiple - 1) >> BufSizeShift) << BufSizeShift;
			u8* new_buf = (u8 *)malloc(new_buf_size);

			buffers.push_back(Buffer(new_buf, new_buf + new_buf_size));
			cur_buf = &buffers.back();
		}

		auto ret = Ref<u8>(cur_buf->end, tl::narrow<u32>(cur_abs_offs));
		cur_buf->end += bytes;
		cur_abs_offs += words;
		return ret;
	}
#endif

	template<typename T>
	Ref<T> alloc(usize count = 1) {
		usize bytes = sizeof(T) * count;
		if ((sizeof(T) & 7) != 0) {
			bytes = ((bytes - 1) | 7) + 1;
		}
		
		auto p = alloc_words(bytes >> 3);
		memset(p.ptr, 0, bytes);
		return Ref<T>(p.ptr, p.abs_offs);
	}

	StringRef alloc_str(tl::StringSlice str) {
		auto m = alloc<u8>(str.size());
		memcpy(m.ptr, str.begin(), str.size());
		return StringRef(m.ptr, m.abs_offs, tl::narrow<u32>(str.size()));
	}

#if SS_BACKWARD
	tl::Vec<u8> to_vec() {
		tl::Vec<u8> ret(cur_abs_offs * 8);
		ret.unsafe_set_size(cur_abs_offs * 8);
		u8* end = ret.end();

		for (auto& buf : buffers) {
			usize written = buf.cap - buf.end;
			end -= written;
			memcpy(end, buf.end, written);
		}

		cur_buf = 0;

		for (auto& buf : buffers) {
			free(buf.begin);
		}
		buffers.clear();

		return std::move(ret);
	}
#else
	tl::Vec<u8> to_vec() {
		tl::Vec<u8> ret(cur_abs_offs * 8);
		ret.unsafe_set_size(cur_abs_offs * 8);
		u8* begin = ret.begin();

		for (auto& buf : buffers) {
			usize written = buf.end - buf.begin;
			memcpy(begin, buf.begin, written);
			begin += written;
		}

		cur_buf = 0;

		for (auto& buf : buffers) {
			free(buf.begin);
		}
		buffers.clear();

		return std::move(ret);
	}
#endif
};

template<typename T>
struct ArrayBuilder : ArrayRef<T> {
	//static_assert((sizeof(T) & 7) == 0, "Arrays only support types with size a multiple of 8");

	ArrayBuilder(Builder& b, usize count)
		: ArrayRef<T>(b.alloc<T>(count), tl::narrow<u32>(count)) {
	}

	ArrayRef<T> done() {
		return std::move(*this);
	}
};

struct Expander {
	tl::BufferMixed buf;
	tl::VecSlice<u8 const> data;

	Expander(tl::VecSlice<u8 const> data)
		: data(data) {
	}

	template<typename T>
	Ref<T> alloc_uninit_raw(usize count = 1) {
		usize size = sizeof(T) * count;
		if ((sizeof(T) & 7) != 0) {
			size = round_size_up(size);
		}
		u8* ptr = this->buf.unsafe_inc_size(size);
		return Ref<T>(ptr, tl::narrow<u32>((ptr - this->buf.begin()) >> 3));
	}

	template<typename To, typename From>
	ArrayRef<To> expand_array_raw(Offset const& from) {
		auto p = (From const *)from.ptr();

		Ref<To> new_arr = this->alloc_uninit_raw<To>(from.size);

		for (u32 i = 0; i < from.size; ++i) {
			To::expand_raw(new_arr + i, *this, p[i]);
		}

		return ArrayRef<To>(new_arr, from.size);
	}

	template<typename To, typename From>
	ArrayRef<To> expand_array_raw_plain(Offset const& from) {
		static_assert(std::is_same<To, From>::value, "types must be the same");

		auto p = (From const *)from.ptr();

		Ref<To> new_arr = this->alloc_uninit_raw<To>(from.size);

		memcpy(new_arr.ptr, p, from.size * sizeof(To));

		return ArrayRef<To>(new_arr, from.size);
	}

	template<typename To, typename From>
	To const* expand_root(From const& from) {
		usize cur_size = To::calc_extra_size(sizeof(To), *this, from);
		
		this->buf.reserve_bytes(this->buf.size() + cur_size);
		auto ref = this->alloc_uninit_raw<To>();
		To::expand_raw(ref, *this, from);
		assert(this->buf.size() == cur_size);
		return (To const*)ref.ptr;
	}

	template<typename To, typename From>
	usize array_calc_size(usize cur_size, Offset const& from) {
		static_assert((sizeof(To) & 7) == 0, "To type must be 8-byte aligned");
		static_assert((sizeof(From) & 7) == 0, "From type must be 8-byte aligned");
		auto p = (From const *)from.ptr();

		cur_size += from.size * sizeof(To);

		for (u32 i = 0; i < from.size; ++i) {
			cur_size = To::calc_extra_size(cur_size, *this, p[i]);
		}

		return cur_size;
	}

	template<typename To, typename From>
	usize array_calc_size_plain(usize cur_size, Offset const& from) {
		static_assert(std::is_same<To, From>::value, "types must be the same");

		cur_size += round_size_up(from.size * sizeof(To));

		return cur_size;
	}

	StringRef alloc_str_raw(tl::StringSlice str) {
		usize str_size = str.size();
		usize size = round_size_up(str_size);
		u8* ptr = this->buf.unsafe_inc_size(size);
		memcpy(ptr, str.begin(), str_size);

		return StringRef(ptr,
			tl::narrow<u32>((ptr - this->buf.begin()) >> 3),
			tl::narrow<u32>(str_size));
	}

	tl::BufferMixed to_buffer() {
		return std::move(this->buf);
	}
};

}

#endif
