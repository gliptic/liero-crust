#ifndef BASE_MODEL_HPP
#define BASE_MODEL_HPP 1

#include <tl/platform.h>
#include <tl/string.hpp>
#include <tl/bits.h>
#include <type_traits>
#include "../config.hpp"

namespace ss {

struct Expander;

#define SS_REF_ONLY_PTR 1

struct BaseRef {
	u8* ptr;
#if !SS_REF_ONLY_PTR
	u32 abs_offs;
#endif

#if SS_REF_ONLY_PTR
	BaseRef(u8* ptr_init)
		: ptr(ptr_init) {
	}
#else
	BaseRef(u8* ptr_init, u32 abs_offs)
		: ptr(ptr_init), abs_offs(abs_offs) {
	}
#endif

#if SS_REF_ONLY_PTR
	TL_FORCE_INLINE BaseRef add_words(usize i) {
		return BaseRef(this->ptr + i * 8);
	}
#else
	TL_FORCE_INLINE BaseRef add_words(usize i) {
		return BaseRef(this->ptr + i * 8, this->abs_offs + i);
	}
#endif
};

template<typename T>
struct Ref : BaseRef {

	Ref(Ref const&) = default;
	Ref& operator=(Ref const&) = default;

#if SS_REF_ONLY_PTR
	Ref(u8* ptr_init)
		: BaseRef(ptr_init) {
	}
#else
	Ref(u8* ptr_init, u32 abs_offs)
		: BaseRef(ptr_init, abs_offs) {
	}
#endif

	Ref(Ref&& other)
#if SS_REF_ONLY_PTR
		: BaseRef(other.ptr) {

		other.ptr = 0;
#else
		: BaseRef(other.ptr, other.abs_offs) {

		other.abs_offs = 0;
		other.ptr = 0;
#endif
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
#if SS_REF_ONLY_PTR
		return Ref<U>(ptr + Offset);
#else
		return Ref<U>(ptr + Offset, abs_offs + u32(Offset / 8));
#endif
	}

	TL_FORCE_INLINE Ref<T> operator+(usize i) {
#if SS_REF_ONLY_PTR
		return Ref<T>(this->ptr + i * sizeof(T));
#else
		return Ref<T>(this->ptr + i * sizeof(T), this->abs_offs + tl::narrow<u32>(i * (sizeof(T) / 8)));
#endif
	}
#endif

	Ref<T> clone() {
		return Ref<T>(this->ptr);
	}
};

struct StringRef : Ref<u8> {
	u32 size;

#if SS_REF_ONLY_PTR
	StringRef(u8* ptr_init, u32 size_init)
		: Ref(ptr_init), size(size_init) {
#else
	StringRef(u8* ptr_init, u32 abs_offs, u32 size_init)
		: Ref(ptr_init, abs_offs), size(size_init) {
#endif
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
		auto rel_offs_new = p - (u8 *)this;
		assert(rel_offs_new >= 0);

		this->rel_offs = tl::narrow<u32>(rel_offs_new >> 3); // TODO: Check
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
#if SS_REF_ONLY_PTR
		this->set((value.ptr - self_ref.ptr) >> 3, value.size);
#else
		this->set(value.abs_offs - self_ref.abs_offs, value.size);
#endif
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
#if SS_REF_ONLY_PTR
		this->set((value.ptr - self_ref.ptr) >> 3, value.size);
#else
		this->set(value.abs_offs - self_ref.abs_offs, value.size);
#endif
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
#if SS_REF_ONLY_PTR
		this->set((value.ptr - self_ref.ptr) >> 3, sizeof(T) / 8);
#else
		this->set(value.abs_offs - self_ref.abs_offs, sizeof(T) / 8);
#endif
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

#if SS_REF_ONLY_PTR
		auto ret = Ref<u8>(cur_buf->end);
#else
		auto ret = Ref<u8>(cur_buf->end, tl::narrow<u32>(cur_abs_offs));
#endif
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
#if SS_REF_ONLY_PTR
		return Ref<T>(p.ptr);
#else
		return Ref<T>(p.ptr, p.abs_offs);
#endif
	}

	StringRef alloc_str(tl::StringSlice str) {
		auto m = alloc<u8>(str.size());
		memcpy(m.ptr, str.begin(), str.size());
#if SS_REF_ONLY_PTR
		return StringRef(m.ptr, tl::narrow<u32>(str.size()));
#else
		return StringRef(m.ptr, m.abs_offs, tl::narrow<u32>(str.size()));
#endif
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

	u64* word(u32 offset) {
		return (u64 *)buf.begin() + offset;
	}

	template<typename T>
	u32 alloc_uninit(usize count = 1) {
		usize size = sizeof(T) * count;
		if ((sizeof(T) & 7) != 0) {
			size = round_size_up(size);
		}
		u8* ptr = this->buf.unsafe_alloc(size);
		return tl::narrow<u32>((ptr - this->buf.begin()) >> 3);
	}

	template<typename T>
	Ref<T> alloc_uninit_raw(usize count = 1) {
		usize size = sizeof(T) * count;
		if ((sizeof(T) & 7) != 0) {
			size = round_size_up(size);
		}
		u8* ptr = this->buf.unsafe_inc_size(size);
#if SS_REF_ONLY_PTR
		return Ref<T>(ptr);
#else
		return Ref<T>(ptr, tl::narrow<u32>((ptr - this->buf.begin()) >> 3));
#endif
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

#if SS_REF_ONLY_PTR
		return StringRef(ptr, tl::narrow<u32>(str_size));
#else
		return StringRef(ptr,
			tl::narrow<u32>((ptr - this->buf.begin()) >> 3),
			tl::narrow<u32>(str_size));
#endif
	}

	tl::BufferMixed to_buffer() {
		return std::move(this->buf);
	}

	enum FieldKind : u8 {
		FieldInlineStruct = 0,
		FieldArrayOfStruct = 1,
		FieldPodArray = 2,
		FieldString = 3,
		FieldArrayOfString = 4,
		FieldIndirectStruct = 5,
	};

	/*
	u64 op (src_word_count:24, dest_word_count:24)
	u64 expanded_flags[(src_word_count + 63) / 64]
	u64 default_data[src_word_count]
	// if expanded:
	//    default_data is (rel_offs: 32, expand_op: 8)
	*/



	u32 expand_exec_alloc(u32 dest, u64 const* program, ss::Offset const& src) {

		u8 const *s = src.ptr();
		u32 src_size = src.size;

		{
			u64 op = *program++;
			u32 src_word_count = op & 0xffffff;
			//u32 dest_word_count = (op >> 24) & 0xffffff;

			u64 const *flags = program;
			u64 const *default_data = program + (src_word_count + 63) / 64;

			auto destp = dest;

			for (u32 i = 0; i < src_word_count; ++i) {
				u8 expanded = (flags[i >> 6] >> (i & 63)) & 1;
				u64 info = *default_data++;
				if (expanded) {
					// Expanding struct in-place

					i32 rel_offs = i32(info & 0xffffffff);

					switch ((info >> 32) & 0xff) {
					case FieldInlineStruct: { // Inline struct
						u64 const* subprogram = program + rel_offs;
						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];
							destp = this->expand_exec_alloc(destp, subprogram, offs);
						} else {
							ss::Offset dummy_offset;
							dummy_offset.set(u32(0), 0);
							destp = this->expand_exec_alloc(destp, subprogram, dummy_offset);
						}
						break;
					}

					case FieldIndirectStruct: {
						u64 const* subprogram = program + rel_offs;

						u64 subheader = *subprogram;
						u32 sub_dest_word_count = (subheader >> 24) & 0xffffff;
						auto struc = this->alloc_uninit<u64>(sub_dest_word_count);

						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];

							this->expand_exec_alloc(struc, subprogram, offs);
							((ss::Offset *)this->word(destp))->set(struc - destp, offs.size);
						} else {
							ss::Offset dummy_offset;
							dummy_offset.set(u32(0), 0);
							this->expand_exec_alloc(struc, subprogram, dummy_offset);
							((ss::Offset *)this->word(destp))->set(struc - destp, sub_dest_word_count);
						}
						++destp;

						break;
					}

					case FieldArrayOfStruct: { // Array of struct
						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];

							u64 const* subprogram = program + rel_offs;
							u64 subheader = *subprogram;
							u32 sub_dest_word_count = (subheader >> 24) & 0xffffff;

							usize array_size = sub_dest_word_count * offs.size; // TODO: Overflow check
							auto arr = this->alloc_uninit<u64>(array_size);

							auto p = arr;
							ss::Offset *sub_src = (ss::Offset *)offs.ptr();
							for (u32 j = 0; j < offs.size; ++j) {
								p = this->expand_exec_alloc(p, subprogram, *sub_src);
								++sub_src;
							}

							((ss::Offset *)this->word(destp))->set(arr - destp, offs.size);

						} else {
							// Empty array
							((ss::Offset *)this->word(destp))->set(u32(0), 0);
						}

						++destp;
						break;
					}

					case FieldArrayOfString: {
						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];

							usize array_size = 8 * offs.size; // TODO: Overflow check
							auto arr = this->alloc_uninit<u64>(array_size);

							auto p = arr;
							ss::Offset *sub_src = (ss::Offset *)offs.ptr();

							for (u32 j = 0; j < offs.size; ++j) {
								usize string_size_bytes = sub_src->size;
								auto dest_str = this->alloc_uninit<u8>(string_size_bytes);

								u8 const *src_str = sub_src->ptr();
								memcpy(this->word(dest_str), src_str, string_size_bytes);

								((ss::Offset *)this->word(p))->set(dest_str - p, sub_src->size);

								++p;
								++sub_src;
							}

							((ss::Offset *)this->word(destp))->set(arr - destp, offs.size);

						} else {
							// Empty array
							((ss::Offset *)this->word(destp))->set(u32(0), 0);
						}

						++destp;
						break;
					}

					case FieldPodArray: { // Plain array
						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];

							u32 sub_dest_byte_count = info & 0xffffffff;

							usize array_size_bytes = sub_dest_byte_count * offs.size; // TODO: Overflow check
							auto arr = this->alloc_uninit<u8>(array_size_bytes);

							u8 const *sub_src = offs.ptr();
							memcpy(this->word(arr), sub_src, array_size_bytes);

							((ss::Offset *)this->word(destp))->set(arr - destp, offs.size);

						} else {
							// Empty array
							((ss::Offset *)this->word(destp))->set(u32(0), 0);
						}

						++destp;
						break;
					}

					case FieldString: {
						if (i < src_size) {
							auto const& offs = ((ss::Offset const *)s)[i];

							usize string_size_bytes = offs.size;
							auto dest_str = this->alloc_uninit<u8>(string_size_bytes);

							u8 const *src_str = offs.ptr();
							memcpy(this->word(dest_str), src_str, string_size_bytes);

							((ss::Offset *)this->word(destp))->set(dest_str - destp, offs.size);

						} else {
							// Empty string
							((ss::Offset *)this->word(destp))->set(u32(0), 0);
						}

						++destp;
						break;
					}

					}
				} else {
					*this->word(destp) = info ^ (i < src_size ? ((u64 const *)s)[i] : 0);
					++destp;
				}
			}

			return destp;
		}
	}

	u64* expand(u64 const* program, u8 const* src) {

		u64 op = *program;
		u32 dest_word_count = (op >> 24) & 0xffffff;
		auto dest = this->alloc_uninit<u64>(dest_word_count);

		this->expand_exec_alloc(dest, program, *(ss::Offset const*)src);
		return this->word(dest);
	}

};

}

#endif
