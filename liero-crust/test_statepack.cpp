#include <tl/cstdint.h>
#include <tl/std.h>
#include <tl/io/stream.hpp>

struct Arith {
	u32 low, high, x;
	u8 *buf, *end;
	int mode;

	void start_encode() {
		high = 0xffffffff;
		low = 0;
		x = 0;
		mode = 1;
	}

	void start_decode() {
		high = low = mode = 0;
		x = 0;
		bit(0);
	}

	u32 bit(u32 p, u32 b = 0) {
		u32 mid = low + u32(u64(high - low) * p >> 12);
		u32 y = mode ? b : x <= mid;

		if (y) high = mid; else low = mid + 1;
		while (!((low ^ high) >> 24)) {
			if (mode && buf != end) *buf++ = high >> 24;
			low <<= 8;
			high = (high << 8) + 0xff;
			if (!mode) x = (x << 8) + (buf != end ? *buf++ : 0xff);
		}

		return y;
	}

	void flush_encode() {
		if (buf != end) *buf++ = low >> 24;
	}
};

void test_statepack() {
	{
		u8 buf_out[4096], buf_in[4096];

		Arith arith;
		arith.buf = buf_out;
		arith.end = buf_out + 4096;

		arith.start_encode();
		arith.bit(0x10, 1);
		arith.bit(0x100, 1);
		arith.bit(1 << 9, 0);
		for (u32 i = 0; i < 5; ++i) {
			arith.bit(1 << 11, 1);
			arith.bit(1 << 10, 0);
			arith.bit(7777, 1);
		}
		arith.flush_encode();

		arith.end = arith.buf;
		arith.buf = buf_out;

		arith.start_decode();
		assert(arith.bit(0x10) == 1);
		assert(arith.bit(0x100) == 1);
		assert(arith.bit(1 << 9) == 0);
		for (u32 i = 0; i < 5; ++i) {
			assert(arith.bit(1 << 11) == 1);
			assert(arith.bit(1 << 10) == 0);
			assert(arith.bit(7777) == 1);
		}
	}

	auto src = tl::Source::from_file("webliero-state.txt");
	auto in_buf = src.read_all();

	{
		tl::Vec<u8> out_buf(in_buf.size(), 0);
		Arith output;
		output.buf = out_buf.begin();
		output.end = out_buf.end();

		output.start_encode();
		u32 context = 0;
		tl::Vec<u16> model(0x1000 * 256 + 0xff * 256 + 256, 2048);
		tl::Vec<u16> count(0x1000 * 256 + 0xff * 256 + 256, 1);

		auto predict = [&](u32 context, u32 bit) -> u32 {
			u32 p = model[context];
			auto next_count = count[context] + 1;
			model[context] = (p * count[context] + (bit ? 4095 : 1)) / next_count;
			count[context] = next_count;
			return p;
		};

		auto abs = [](i32 v) {
			return v < 0 ? -v : v;
		};

		auto max_cert = [&](u32 a, u32 b) {
			return abs((i32)a - 2048) > abs((i32)b - 2048) ? a : b;
		};

		u32 index = 0;
		for (auto byte : in_buf) {
			for (u32 k = 0; k < 8; ++k) {
				u32 context2 = (byte & ((1 << k) - 1)) + (1 << k);
				u32 c = (context & 0xff) * 256 + context2;
				u32 c2 = (context & 0xff);
				c2 = (0xff*256 + 256) + ((c2*c2 >> 8) & 0xfff) * 256 + context2;
				u32 b = (byte >> k) & 1;
				u32 p = max_cert(predict(c, b), predict(c2, b));

				output.bit(p, b);

				if (output.buf == output.end) {
					output.buf = output.end;
				}
			}
			context = (context << 8) | byte;
			++index;
		}
		output.flush_encode();

		printf("%d\n", (int)in_buf.size());
		printf("%d\n", output.buf - out_buf.begin());
	}
}