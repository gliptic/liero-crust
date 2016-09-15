#include <tl/std.h>
#include <tl/memory.h>
#include <tl/vec.hpp>
#include <intrin.h>

namespace rans {

template<typename T>
inline T bit_mask(u32 bits) {
	return T((T(1) << bits) - 1);
}

struct SymRange {
	u16 start, freq;

	SymRange(u16 start, u16 freq) : start(start), freq(freq) {
	}
};

template<usize Count>
struct ModelBits {
	u16 cm[Count + 1];

	ModelBits(bool oldmodel) {
		if (oldmodel) {
			for (usize i = 0; i < Count; ++i) {
				cm[i] = i16((i * 0x8000) / Count);
			}

			cm[Count] = 0x8000;
		} else {
			for (usize i = 0; i < Count; ++i) {
				cm[i] = i16(((Count - i) * 0x8000) / Count);
			}

			cm[Count] = 0;
		}
	}

	void update(usize sym) {

		for (usize j = 1; j < Count; ++j) {
#if 0
			i16 adjustment = i16((j > sym ? 0x7FD9 : 0) + 8*j - cm[j]);
			cm[j] += adjustment >> 7;
#else
			// step = (1 << n) - m + (1 << rate)
			// mcm = j - (1 << rate) + (j > sym ? step : 0)
			// cm = cm + (mcm - cm) >> rate

			u16 mcm = u16((j > sym ? 0x7fff - 0xf : 0) + j);
			
			// 1, 1+step, 2+step, 3+step, 2^n-1+step
			
			// 2^n-1+step = 2^m-1
			// step = 32767 - 15

			i16 adjustment = i16(cm[j] - mcm);
			cm[j] -= adjustment >> 7;
#endif
		}

		
	}

	SymRange get(usize sym) {
		return SymRange(cm[sym], cm[sym + 1] - cm[sym]);
	}
};


template<typename State, usize Count>
static TL_FORCE_INLINE usize read_bits(typename State::WordType*& cur, State& state, ModelBits<Count>& model) {

	typename State::StateType x = state.x;

	usize const scale_log = 15;

	u16 point = u16(x) & bit_mask<u16>(scale_log);

	usize i = 1;
	for (; i < Count; ++i) {
		if (point < model.cm[i]) {
			break;
		}
	}

	usize sym = i - 1;
	i16 start = model.cm[sym];
	i16 end = model.cm[i];

	model.update(sym);
	state.advance(cur, start, end - start, 15);
	return sym;
}

/*
template<typename State, usize Count>
static TL_FORCE_INLINE void write_bits(typename State::WordType*& cur, State& state, ModelBits<Count>& model, usize sym) {

	typename State::StateType x = state.x;

	usize const scale_log = 15;

	i16 start = model.cm[sym];
	i16 end = model.cm[sym + 1];

	state.put(end, start, end - start, scale_log);
	model.update(sym);
}*/

template<typename StateT, typename WordT, u32 RenormLog>
struct EncState {
	StateT x;

	typedef StateT StateType;
	typedef WordT WordType;

	static StateT const renorm_tresh = 1u << RenormLog;
	static usize const state_bits = sizeof(StateT) * 8;
	static usize const word_bits = sizeof(WordT) * 8;
	static StateT const word_mask = (StateT(1) << word_bits) - 1;

	EncState(StateT init) : x(init) {}

	//// Encode

	static EncState start_encode() {
		return EncState(renorm_tresh);
	}

	StateT enc_renorm(WordT*& end, u32 freq, u32 scale_log) const {
		StateT lx = this->x;
		StateT x_max = ((renorm_tresh >> scale_log) << word_bits) * freq;

		if (scale_log < word_bits) {
			end[-1] = WordT(lx & word_mask);
			if (lx >= x_max) {
				lx >>= word_bits;
				--end;
			}
				
		} else {
			while (lx >= x_max) {
				*--end = WordT(lx & word_mask);
				lx >>= word_bits;
			}
		}

		return lx;
	}

	void put(WordT*& end, u32 start, u32 freq, u32 scale_log) {
		auto x2 = enc_renorm(end, freq, scale_log);
		this->x = ((x2 / freq) << scale_log) + (x2 % freq) + start;
	}

	void put_bit(WordT*& end, u32 bit, u32 scale_log) {

		StateT x2 = this->x;
		StateT x_max = (renorm_tresh >> scale_log) << word_bits;
		while (x2 >= x_max) {
			*--end = WordT(x2 & word_mask);
			x2 >>= word_bits;
		}

		this->x = (x2 << scale_log) + bit;
	}

	void flush_naive(WordT*& end) const {
		static_assert((state_bits % word_bits) == 0, "state size must be multiple of word size");
		WordT* p = end - state_bits / word_bits;
		tl::write_le<StateT>(p, x);
		end = p;
	}
};

template<typename StateT, typename WordT, u32 RenormLog>
struct DecState {
	StateT x;

	typedef StateT StateType;
	typedef WordT WordType;

	static StateT const renorm_tresh = 1u << RenormLog;
	static usize const state_bits = sizeof(StateT) * 8;
	static usize const word_bits = sizeof(WordT) * 8;
	static StateT const word_mask = (StateT(1) << word_bits) - 1;

	DecState(StateT init) : x(init) {}

	//// Decode

	static DecState start_decode_naive(WordT*& cur) {
		WordT *p = cur;
		StateT lx = tl::read_le<StateT>(p);
		cur = p + state_bits / word_bits;
		return DecState(lx);
	}

	u32 get(u32 scale_log) const {
		return this->x & bit_mask<u32>(scale_log);
	}

	void advance(WordT*& cur, u32 start, u32 freq, u32 scale_log) {
		StateT mask = bit_mask<StateT>(scale_log);

		StateT lx = this->x;
		this->x = freq * (lx >> scale_log) + (lx & mask) - start;

		dec_renorm(cur, scale_log);
	}

	void dec_renorm(WordT*& cur, u32 scale_log) {
		if (scale_log < word_bits) {
			StateT next = (this->x << word_bits) | *cur;

			if (this->x < renorm_tresh) {
				this->x = next;
				++cur;
			}
		} else {
			while (this->x < renorm_tresh) {
				this->x = (this->x << word_bits) | *cur++;
			}
		}
	}
};

typedef EncState<u32, u8, 23> ByteEncState;
typedef EncState<u64, u32, 31> WideEncState;
typedef DecState<u64, u32, 31> WideDecState;

#if 1
template<typename State, usize Count>
TL_FORCE_INLINE u32 read_bits_2(typename State::WordType*& cur, State& state, ModelBits<Count>& model);

#define OFFSET(x) ((x))

template<>
TL_FORCE_INLINE u32 read_bits_2<WideDecState, 16>(u32*& cur, WideDecState& state, ModelBits<16>& model) {
	u64 x = state.x;

	__m128i t0 = _mm_loadu_si128((const __m128i *)&model.cm[0]);
	__m128i t1 = _mm_loadu_si128((const __m128i *)&model.cm[8]);

	__m128i t = _mm_cvtsi32_si128(x & 0x7FFF);
	t = _mm_shuffle_epi32(_mm_unpacklo_epi16(t, t), 0);

	__m128i m0 = _mm_cmpgt_epi16(t0, t);
	__m128i m1 = _mm_cmpgt_epi16(t1, t);
	
	unsigned long bitindex;
	_BitScanForward(&bitindex, _mm_movemask_epi8(_mm_packs_epi16(m0, m1)) | 0x10000);
	u16 start = model.cm[bitindex - 1];
	u16 end = model.cm[bitindex];

	m0 = _mm_and_si128(_mm_set1_epi16(0x7fff - 0xf), m0);
	m1 = _mm_and_si128(_mm_set1_epi16(0x7fff - 0xf), m1);

	m0 = _mm_add_epi16(m0, _mm_set_epi16(OFFSET(7), OFFSET(6), OFFSET(5), OFFSET(4), OFFSET(3), OFFSET(2), OFFSET(1), OFFSET(0)));
	m1 = _mm_add_epi16(m1, _mm_set_epi16(OFFSET(15), OFFSET(14), OFFSET(13), OFFSET(12), OFFSET(11), OFFSET(10), OFFSET(9), OFFSET(8)));

	t0 = _mm_sub_epi16(t0, _mm_srai_epi16(_mm_sub_epi16(t0, m0), 7));
	t1 = _mm_sub_epi16(t1, _mm_srai_epi16(_mm_sub_epi16(t1, m1), 7));

	_mm_storeu_si128((__m128i *)&model.cm[0], t0);
	_mm_storeu_si128((__m128i *)&model.cm[8], t1);

	state.advance(cur, start, end - start, 15);
	return (u32)bitindex - 1;
}

#endif

TL_NEVER_INLINE void test() {

#if 0
	u8 buf[256];

	u8* p = buf + 256;
	auto e = ByteState::start_encode();

	e.put(p, 0, 1, 8);
	e.put(p, 5, 1, 8);
	e.flush_naive(p);

	auto d = ByteState::start_decode_naive(p);
	u32 s0 = d.get(8); d.advance(p, s0, 1, 8);
	u32 s1 = d.get(8); d.advance(p, s1, 1, 8);

	assert(s0 == 5 && s1 == 0);
#else
	u32 buf[256];

	tl::Vec<SymRange> symbols;

	{
		ModelBits<16> model(true);

		for (int i = 0; i < 10000; ++i) {
			model.update(11);
		}

		for (int i = 0; i < 10; ++i) {
			usize sym = i;
			symbols.push_back(model.get(sym));
			model.update(sym);
		}
	}

	u32* p = buf + 256;
	{
		auto e = WideEncState::start_encode();
		for (usize i = symbols.size(); i-- > 0; ) {
			e.put(p, symbols[i].start, symbols[i].freq, 15);
		}

		e.flush_naive(p);
	}

	{
		ModelBits<16> model(true);

		for (int i = 0; i < 10000; ++i) {
			model.update(11);
		}

		auto d = WideDecState::start_decode_naive(p);

		for (int i = 0; i < 10; ++i) {
			printf("%d\n", (int)read_bits_2(p, d, model));
		}
	}

#endif
}

}