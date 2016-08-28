#include <tl/std.h>
#include <tl/bits.h>
#include <tl/vec.hpp>
#include <tl/memory.h>

#define FSE_MAX_MEMORY_USAGE (14)
#define FSE_DEFAULT_MEMORY_USAGE (13)
#define FSE_MAX_SYMBOL_VALUE (255)
#define FSE_MIN_TABLELOG (5)
#define FSE_MAX_TABLELOG  (FSE_MAX_MEMORY_USAGE-2)
#define FSE_MAX_TABLESIZE (1U<<FSE_MAX_TABLELOG)
#define FSE_MAXTABLESIZE_MASK (FSE_MAX_TABLESIZE-1)

#define FSE_DEFAULT_TABLELOG (FSE_DEFAULT_MEMORY_USAGE-2)

// 5x/8 + 3
#define FSE_TABLESTEP(table_size) (((table_size)>>1) + ((table_size)>>3) + 3)

namespace fse {

typedef u8 Symbol;

struct BaseSymbolTable;

struct BaseStateTable {
	u16 table_log;
	u16 max_symbol_value;

	u16* vals() { return (u16 *)(this + 1);	}
	u16 const* vals() const { return (u16 const *)(this + 1); }

	usize build(BaseSymbolTable* symbol_tab, i16 const* normalized_counter, u32 max_symbol_value, u32 table_log);
	u32 compress(BaseSymbolTable const* symbol_tab, tl::VecSlice<Symbol const> src, tl::VecSlice<u8> dest);
};

template<usize Size = FSE_MAX_TABLESIZE>
struct StateTable : BaseStateTable {
	u16 storage[Size];
};


struct SymbolDecoder {
	u16 new_state;
	Symbol symbol;
	u8 nb_bits;
};

struct BaseStateDecodeTable {
	u16 table_log, fast_mode;

	SymbolDecoder* vals() { return (SymbolDecoder *)(this + 1);	}
	SymbolDecoder const* vals() const { return (SymbolDecoder const *)(this + 1); }

	usize build(i16 const* normalized_counter, u32 max_symbol_value, u32 table_log);
	u32 decompress(tl::VecSlice<Symbol const> src, tl::VecSlice<Symbol> dest);
};

template<usize Size = FSE_MAX_TABLESIZE>
struct StateDecodeTable : BaseStateDecodeTable {
	SymbolDecoder storage[Size];
};

struct SymbolTransform {
	i32 delta_find_state;
	u32 delta_nb_bits;
};

struct BaseSymbolTable {
	SymbolTransform* vals() { return (SymbolTransform *)this; }
	SymbolTransform const* vals() const { return (SymbolTransform const *)this; }
};

template<usize Size = FSE_MAX_SYMBOL_VALUE + 1>
struct SymbolTable : BaseSymbolTable {
	SymbolTransform storage[Size];
};

usize BaseStateTable::build(BaseSymbolTable* symbol_tab, i16 const* normalized_counter, u32 lmax_symbol_value, u32 ltable_log) {
	u32 const table_size = 1 << ltable_log;
	
	u32 cumul[FSE_MAX_SYMBOL_VALUE + 1];
	Symbol index_to_symbol[FSE_MAX_TABLESIZE];
	u32 top = table_size;

	this->table_log = u16(ltable_log);
	this->max_symbol_value = u16(lmax_symbol_value);

	/*
	* cumul[n] = sum(count[0..n-1])
	* Allocate low-prob symbols to highest indexes
	* Initialize symbol transformations
	*/

	for (u32 s = 0, sum = 0; s <= lmax_symbol_value; ++s) {
		cumul[s] = sum;

		u32 c = normalized_counter[s];

		if (c == -1) {
			c = 1;
			index_to_symbol[--top] = Symbol(s);
		}

		if (c != 0) {
			// TODO: This branch could be unpredictable. Is it better to run this code in all cases? It doesn't hurt
			u32 const max_bits_out = ltable_log - tl_log2(c - 1);
			u32 const min_state_plus = c << max_bits_out;

			symbol_tab->vals()[s].delta_nb_bits = (max_bits_out << 16) - min_state_plus;
			symbol_tab->vals()[s].delta_find_state = sum - c;
		}

		sum += c;
	}

	// Allocate remaining symbols in spread out pattern

	{
		u32 const step = FSE_TABLESTEP(table_size);
		u32 const table_mask = table_size - 1;
		u32 position = 0;
		for (u32 s = 0; s <= lmax_symbol_value; ++s) {
			for (i32 i = 0; i < normalized_counter[s]; ++i) {
				
				index_to_symbol[position] = Symbol(s);

				do
					position = (position + step) & table_mask;
				while (position >= top);
			}
		}

		assert(position == 0);
	}

	// Build state transition table
	for (u32 u = 0; u < table_size; ++u) {
		Symbol s = index_to_symbol[u];
		this->vals()[cumul[s]++] = u16(table_size + u);
	}

	return 0;
}

usize BaseStateDecodeTable::build(i16 const* normalized_counter, u32 max_symbol_value, u32 ltable_log) {
	u32 const table_size = 1 << ltable_log;

	u16 symbol_next[FSE_MAX_SYMBOL_VALUE + 2];
	u32 top = table_size;

	this->table_log = u16(ltable_log);
	this->fast_mode = 1;

	u32 const large_limit = table_size >> 1;

	for (u32 s = 0; s <= max_symbol_value; ++s) {
		u32 c = normalized_counter[s];

		if (c == -1) {
			c = 1;
			this->vals()[--top].symbol = tl::narrow<Symbol>(s);
		} else if (c >= large_limit) {
			this->fast_mode = 0;
		}

		symbol_next[s] = tl::narrow<Symbol>(c);
	}

	{
		u32 const step = FSE_TABLESTEP(table_size);
		u32 const table_mask = table_size - 1;
		u32 position = 0;
		for (u32 s = 0; s <= max_symbol_value; ++s) {
			for (i32 i = 0; i < normalized_counter[s]; ++i) {
				
				this->vals()[position].symbol = Symbol(s); // This is the only difference between encode/decode spread

				do
					position = (position + step) & table_mask;
				while (position >= top);
			}
		}

		assert(position == 0);
	}

	for (u32 u = 0; u < table_size; ++u) {
		Symbol const s = this->vals()[u].symbol;
		u16 next_state = symbol_next[s]++;
		u8 nb_bits = u8(table_log - tl_fls(next_state));
		this->vals()[u].nb_bits = nb_bits;
		this->vals()[u].new_state = u16((next_state << nb_bits) - table_size);
	}

	return 0;
}

u32 count_simple(u32* count, u32* max_symbol_value_ptr, Symbol const* src, usize src_size) {

	Symbol const* cur = src;
	Symbol const* const end = src + src_size;

	u32 max_symbol_value = *max_symbol_value_ptr;

	memset(count, 0, (max_symbol_value + 1)*sizeof(*count));
	if (src_size == 0) { *max_symbol_value_ptr = 0; return 0; }

	while (cur != end) count[*cur++]++;

	while (!count[max_symbol_value]) max_symbol_value--;
	*max_symbol_value_ptr = max_symbol_value;

	u32 max = 0;
	for (u32 s = 0; s <= max_symbol_value; ++s) {
		if (count[s] > max)
			max = count[s];
	}

	return max;
}

static u32 const bit_mask[] = { 0, 1, 3, 7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF, 0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF,  0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF };   /* up to 26 bits */

struct BitStream {
	usize bit_container;
	u32 bit_pos;
	u8 *begin, *cur, *end;

	BitStream(tl::VecSlice<Symbol> dest)
		: bit_container(0), bit_pos(0)
		, begin(dest.begin()), cur(dest.begin()), end(dest.end() - sizeof(this->bit_container))  {

	}

	void add(usize value, u32 nb_bits) {
		//this->bit_container |= (value & bit_mask[nb_bits]) << this->bit_pos;
		this->bit_container |= (value & ((u32(1) << nb_bits) - 1)) << this->bit_pos;
		this->bit_pos += nb_bits;
	}

	void add_fast(usize value, u32 nb_bits) {
		this->bit_container |= value << this->bit_pos;
		this->bit_pos += nb_bits;
	}

	void flush_bits_fast() {
		u32 nb_bytes = this->bit_pos >> 3;
		tl::write_le<usize>(this->cur, this->bit_container);
		this->cur += nb_bytes;
		this->bit_pos &= 7;
		this->bit_container >>= nb_bytes * 8;
	}

	void close() {
		add_fast(1, 1);
		flush_bits_fast(); // TODO: Not fast in org
	}
};

struct InBitStream {
	usize bit_container;
	u32 bits_consumed;
	u8 *begin, *cur;

	usize look_bits(u32 nb_bits) {
		u32 const mask = sizeof(bit_container)*8 - 1;
		return ((this->bit_container << (this->bits_consumed & mask)) >> 1) >> ((mask - nb_bits) & mask);
	}

	void skip_bits(u32 nb_bits) {
		this->bits_consumed += nb_bits;
	}

	usize read_bits(u32 nb_bits) {
		usize const value = look_bits(nb_bits);
		skip_bits(nb_bits);
		return value;
	}

	void reload() {
		if (this->cur >= this->begin + sizeof(bit_container)) {
			u32 nb_bytes = this->bits_consumed >> 3;
			this->cur -= nb_bytes;
			this->bits_consumed &= 7;
			this->bit_container = tl::read_le<usize>(this->cur);
		} else if (this->cur == this->begin) {

		}
	}
};

struct State {
	isize value; // TODO: Make this usize and change other types to match
	u16 const* state_tab;
	SymbolTransform const* symbol_tab;
	u32 state_log;

	State(BaseStateTable const* state_tab_init, BaseSymbolTable const* symbol_tab_init)
		: state_tab(state_tab_init->vals())
		, symbol_tab(symbol_tab_init->vals())
		, value(isize(1) << state_tab_init->table_log)
		, state_log(state_tab_init->table_log) {
	}

	void encode_first(Symbol sym) {
		auto tt = this->symbol_tab[sym];
		u32 bits_out = (tt.delta_nb_bits + (1<<15)) >> 16;
		this->value = (bits_out << 16) - tt.delta_nb_bits;
		this->value = this->state_tab[(this->value >> bits_out) + tt.delta_find_state];
	}

	void encode(BitStream& dest, Symbol sym) {
		auto tt = this->symbol_tab[sym];
		u32 bits_out = u32((this->value + tt.delta_nb_bits) >> 16);
		dest.add(this->value, bits_out);
		this->value = this->state_tab[(this->value >> bits_out) + tt.delta_find_state];
	}

	void flush(BitStream& dest) {
		dest.add(this->value, this->state_log);
		dest.flush_bits_fast();
	}
};

struct DecodeState {
	usize value;
	SymbolDecoder const* state_tab;

	DecodeState(BaseStateDecodeTable const* state_tab_init)
		: state_tab(state_tab_init->vals()) {
		
	}
};

u32 BaseStateTable::compress(BaseSymbolTable const* symbol_tab, tl::VecSlice<Symbol const> src, tl::VecSlice<u8> dest) {
	BitStream str(dest);

	State state(this, symbol_tab);

	if (!src.empty()) {
		state.encode_first(src.unsafe_pop_back());

		while (!src.empty()) {
			Symbol sym = Symbol(src.unsafe_pop_back());
			state.encode(str, sym);
			str.flush_bits_fast();
		}

		state.flush(str);
	}

	return str.cur - dest.begin(); // TODO
}

u32 BaseStateDecodeTable::decompress(tl::VecSlice<Symbol const> src, tl::VecSlice<Symbol> dest) {
	TL_UNUSED(src); TL_UNUSED(dest);
	return 0; // TODO
}

static u32 FSE_minTableLog(size_t srcSize, unsigned maxSymbolValue) {
	u32 minBitsSrc = tl_fls((u32)(srcSize - 1)) + 1;
	u32 minBitsSymbols = tl_fls(maxSymbolValue) + 2;
	u32 minBits = minBitsSrc < minBitsSymbols ? minBitsSrc : minBitsSymbols;
	return minBits;
}

namespace FseNormalize {

typedef u32 U32;
typedef u64 U64;

static bool FSE_isError(size_t v) { return v >= (size_t)-1; }

#define ERROR(x) ((size_t)-1)

static size_t FSE_normalizeM2(short* norm, U32 tableLog, const unsigned* count, size_t total, U32 maxSymbolValue)
{
	U32 s;
	U32 distributed = 0;
	U32 ToDistribute;

	/* Init */
	U32 lowThreshold = (U32)(total >> tableLog);
	U32 lowOne = (U32)((total * 3) >> (tableLog + 1));

	for (s=0; s<=maxSymbolValue; s++) {
		if (count[s] == 0) {
			norm[s]=0;
			continue;
		}
		if (count[s] <= lowThreshold) {
			norm[s] = -1;
			distributed++;
			total -= count[s];
			continue;
		}
		if (count[s] <= lowOne) {
			norm[s] = 1;
			distributed++;
			total -= count[s];
			continue;
		}
		norm[s]=-2;
	}
	ToDistribute = (1 << tableLog) - distributed;

	if ((total / ToDistribute) > lowOne) {
		/* risk of rounding to zero */
		lowOne = (U32)((total * 3) / (ToDistribute * 2));
		for (s=0; s<=maxSymbolValue; s++) {
			if ((norm[s] == -2) && (count[s] <= lowOne)) {
				norm[s] = 1;
				distributed++;
				total -= count[s];
				continue;
		}   }
		ToDistribute = (1 << tableLog) - distributed;
	}

	if (distributed == maxSymbolValue+1) {
		/* all values are pretty poor;
		   probably incompressible data (should have already been detected);
		   find max, then give all remaining points to max */
		U32 maxV = 0, maxC = 0;
		for (s=0; s<=maxSymbolValue; s++)
			if (count[s] > maxC) maxV=s, maxC=count[s];
		norm[maxV] += (short)ToDistribute;
		return 0;
	}

	{
		U64 const vStepLog = 62 - tableLog;
		U64 const mid = (1ULL << (vStepLog-1)) - 1;
		U64 const rStep = ((((U64)1<<vStepLog) * ToDistribute) + mid) / total;   /* scale on remaining */
		U64 tmpTotal = mid;
		for (s=0; s<=maxSymbolValue; s++) {
			if (norm[s]==-2) {
				U64 end = tmpTotal + (count[s] * rStep);
				U32 sStart = (U32)(tmpTotal >> vStepLog);
				U32 sEnd = (U32)(end >> vStepLog);
				U32 weight = sEnd - sStart;
				if (weight < 1)
					return ERROR(GENERIC);
				norm[s] = (short)weight;
				tmpTotal = end;
	}   }   }

	return 0;
}


size_t FSE_normalizeCount (short* normalizedCounter, unsigned tableLog,
						   const unsigned* count, size_t total,
						   unsigned maxSymbolValue)
{
	/* Sanity checks */
	if (tableLog==0) tableLog = FSE_DEFAULT_TABLELOG;
	if (tableLog < FSE_MIN_TABLELOG) return ERROR(GENERIC);   /* Unsupported size */
	if (tableLog > FSE_MAX_TABLELOG) return ERROR(tableLog_tooLarge);   /* Unsupported size */
	if (tableLog < FSE_minTableLog(total, maxSymbolValue)) return ERROR(GENERIC);   /* Too small tableLog, compression potentially impossible */

	{   U32 const rtbTable[] = {     0, 473195, 504333, 520860, 550000, 700000, 750000, 830000 };

		U64 const scale = 62 - tableLog;
		U64 const step = ((U64)1<<62) / total;   /* <== here, one division ! */
		U64 const vStep = 1ULL<<(scale-20);
		int stillToDistribute = 1<<tableLog;
		unsigned s;
		unsigned largest=0;
		short largestP=0;
		U32 lowThreshold = (U32)(total >> tableLog);

		for (s=0; s<=maxSymbolValue; s++) {
			if (count[s] == total) return 0;   /* rle special case */
			if (count[s] == 0) { normalizedCounter[s]=0; continue; }
			if (count[s] <= lowThreshold) {
				normalizedCounter[s] = -1;
				stillToDistribute--;
			} else {
				short proba = (short)((count[s]*step) >> scale);
				if (proba<8) {
					U64 restToBeat = vStep * rtbTable[proba];
					proba += (count[s]*step) - ((U64)proba<<scale) > restToBeat;
				}
				if (proba > largestP) largestP=proba, largest=s;
				normalizedCounter[s] = proba;
				stillToDistribute -= proba;
		}   }
		if (-stillToDistribute >= (normalizedCounter[largest] >> 1)) {
			/* corner case, need another normalization method */
			size_t errorCode = FSE_normalizeM2(normalizedCounter, tableLog, count, total, maxSymbolValue);
			if (FSE_isError(errorCode)) return errorCode;
		}
		else normalizedCounter[largest] += (short)stillToDistribute;
	}

#if 0
	{   /* Print Table (debug) */
		U32 s;
		U32 nTotal = 0;
		for (s=0; s<=maxSymbolValue; s++)
			printf("%3i: %4i \n", s, normalizedCounter[s]);
		for (s=0; s<=maxSymbolValue; s++)
			nTotal += abs(normalizedCounter[s]);
		if (nTotal != (1U<<tableLog))
			printf("Warning !!! Total == %u != %u !!!", nTotal, 1U<<tableLog);
		getchar();
	}
#endif

	return tableLog;
}

}

u32 compress(u32 const* real_counts, u32 real_total, tl::VecSlice<u8 const> src, u32 max_symbol_value, tl::VecSlice<u8> dest) {
	
	StateTable<> state_tab;
	SymbolTable<> symbol_tab;

	u32 total = (1 << FSE_DEFAULT_TABLELOG);

	i16 counts[256];

#if 0
#if 0
	counts[0] = i16(total * prob[0]) + 40;
#else
	counts[0] = real_counts[0] * total / (real_counts[0] + real_counts[1]);
#endif
	counts[1] = total - counts[0];
	state_tab.build(&symbol_tab, counts, max_symbol_value, FSE_DEFAULT_TABLELOG);
#else
	
	FseNormalize::FSE_normalizeCount(counts, FSE_DEFAULT_TABLELOG, real_counts, real_total, max_symbol_value);
	state_tab.build(&symbol_tab, counts, max_symbol_value, FSE_DEFAULT_TABLELOG);
#endif
	

	return state_tab.compress(&symbol_tab, src, dest);
}

void test() {
#if 0
	i16 counts[256];

	for (u32 i = 0; i < 256; ++i) {
		counts[i] = (1 << FSE_DEFAULT_TABLELOG) >> 8;
	}

	StateTable<> state_tab;
	SymbolTable<> symbol_tab;
	
	state_tab.build(&symbol_tab, counts, 255, FSE_DEFAULT_TABLELOG);

	u8 dest[1024];
	char const* test = "Hello!!!";
	tl::VecSlice<u8 const> src((u8 const*)test, (u8 const*)test + 8);

	state_tab.compress(&symbol_tab, src, tl::VecSlice<u8>(dest, dest + 1024));
#endif

	u32 total = (1 << FSE_DEFAULT_TABLELOG);

	static usize const SymCount = 10;

	f64 prob[SymCount] = { };

	for (usize i = 0; i < SymCount; ++i) {
		prob[i] = 1.0 / (i * i + 2.0);
	}

	f64 prob_total = 0.0;
	for (auto p : prob) prob_total += p;

	static int const Size = 30000;
	u8 test[Size];
	u32 real_counts[SymCount] = {};

	for (u32 i = 0; i < Size; ++i) {
		u32 sym = 0;
		f64 r = prob_total * (rand() / f64(RAND_MAX));

		for (f64 p : prob) {
			if (r < p || sym == SymCount - 1) break;
			r -= p;
			++sym;
		}

		test[i] = sym;
		++real_counts[test[i]];
	}

	u32 real_total = 0;
	for (u32 v : real_counts) {
		real_total += v;
	}

	tl::VecSlice<u8 const> src(test);
	u8 dest[Size * 2];

	u32 size = compress(real_counts, real_total, src, SymCount - 1, tl::VecSlice<u8>(dest));

	f64 entropy = 0.0;
	for (auto b : src) {
		entropy += -log(real_counts[b] / f64(real_total)) / log(2);
	}

	printf("%d bytes. Expected: %f\n", size, entropy / 8.0);
}

/*

Minimize Cn * lg(Xn / 2^T)

sum(Xn) = 2^T

*/

}