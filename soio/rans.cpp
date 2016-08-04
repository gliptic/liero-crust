#include <tl/std.h>
#include <tl/memory.h>

namespace rans {

static u32 const byte_l = 1u << 23;

template<typename T>
inline T bit_mask(u32 bits) {
	return T((T(1) << bits) - 1);
}

struct State {
	u32 x;

	State(u32 init) : x(init) {}

	//// Encode

	// Writes at most end[-3..-1]
	u32 enc_renorm(u8*& end, u32 freq, u32 scale_log) const {
		u32 lx = this->x;
		u32 x_max = ((byte_l >> scale_log) << 8) * freq;
		while (lx >= x_max) {
			*--end = u8(lx & 0xff);
			lx >>= 8;
		}

		return lx;
	}

	// Writes at most end[-2..-1]
	void put(u8*& end, u32 start, u32 freq, u32 scale_log) {
		auto x2 = enc_renorm(end, freq, scale_log);
		this->x = ((x2 / freq) << scale_log) + (x2 % freq) + start;
	}

	void put_bit(u8*& end, u32 bit, u32 scale_log) {

		u32 x2 = this->x;
		u32 x_max = (byte_l >> scale_log) << 8;
		while (x2 >= x_max) {
			*--end = u8(x2 & 0xff);
			x2 >>= 8;
		}

		//auto x2 = enc_renorm(end, 1, scale_log);
		this->x = (x2 << scale_log) + bit;
	}

	// Writes end[-4..-1]
	void flush_naive(u8*& end) const {
		u8* p = end - 4;
		tl::write_le<u32>(p, x);
		end = p;
	}


	//// Decode

	static State start_decode_naive(u8*& cur) {
		u8 *p = cur;
		u32 lx = tl::read_le<u32>(p);
		cur = p + 4;
		return State(lx);
	}

	static State start_encode() {
		return State(byte_l);
	}

	u32 get(u32 scale_log) const {
		return this->x & bit_mask<u32>(scale_log);
	}

	void advance(u8*& cur, u32 start, u32 freq, u32 scale_log) {
		u32 mask = bit_mask<u32>(scale_log);

		u32 lx = this->x;
		lx = freq * (lx >> scale_log) + (lx & mask) - start;

		while (lx < byte_l) {
			lx = (lx << 8) | *cur++;
		}

		this->x = lx;
	}
};

void test() {
	u8 buf[256];

	u8* p = buf + 256;

	auto e = State::start_encode();

	e.put(p, 0, 1, 8);
	e.put(p, 5, 1, 8);
	e.flush_naive(p);

	auto d = State::start_decode_naive(p);
	u32 s0 = d.get(8); d.advance(p, s0, 1, 8);
	u32 s1 = d.get(8); d.advance(p, s1, 1, 8);

	assert(s0 == 5 && s1 == 0);
}

}