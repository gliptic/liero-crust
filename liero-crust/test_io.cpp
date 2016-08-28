#include <tl/vec.hpp>
#include <tl/memory.h>

struct Buffer {
	u8* read;
	u8* write;
	u8* end;

	usize left_to_write() const { return end - write; }
	usize left_to_read() const { return write - read; }

	bool allow_overwrite; // true if sink is allowed to overwrite [read, write)

	bool validate() const {
		return read <= write && write <= end;
	}

	Buffer() : read(0), write(0), end(0), allow_overwrite(false) {
	}
};

struct Sink {

	Buffer in;

	virtual void pump() { }
};

struct Pusher {
	Sink* next;
	u8* next_buf;

	Pusher(Sink* next_init = 0) : next(next_init), next_buf(0) {
	}

	~Pusher() {
		free(next_buf);
	}

	u8* require_write_space(usize wanted_write_space = 1) {
		Buffer& out = next->in;

		if (out.left_to_write() < wanted_write_space) {
			// Try to make space

			usize left_in_out = out.left_to_read();

			{
				usize min_buf_size = left_in_out + wanted_write_space;

				usize new_buf_size = (min_buf_size + 1023) & ~usize(1023);

				next_buf = (u8 *)realloc(next_buf, new_buf_size);
				out.end = next_buf + new_buf_size;
			}

			memmove(next_buf, out.read, left_in_out);

			out.read = next_buf;
			out.write = next_buf + left_in_out;
			out.allow_overwrite = true;
		}

		assert(out.validate());
		assert(out.left_to_write() >= wanted_write_space);

		return out.write;
	}
};

struct Filter : Sink, Pusher {
	Filter(Sink* next_init = 0) : Pusher(next_init) {
	}

	u8* require_write_space_inplace(usize wanted_write_space = 1) {

		Buffer& out = next->in;

		if (out.write == in.read) {
			out.end = in.write;
		}

		if (out.left_to_write() < wanted_write_space) {
			// Try to make space

			usize left_in_out = out.left_to_read();

			if (left_in_out == 0 && in.allow_overwrite && wanted_write_space <= in.left_to_read()) {

				out.read = out.write = in.read;
				out.end = in.write; // Can write up to write cursor
				out.allow_overwrite = in.allow_overwrite;

			} else {
				return require_write_space(wanted_write_space);
			}
		}

		assert(out.validate());
		assert(out.left_to_write() >= wanted_write_space);

		return out.write;
	}
};

struct AddOnFilter : Filter {

	AddOnFilter(Sink* next_init = 0) : Filter(next_init) {
	}

	void pump() {

		Buffer& out = next->in;
		assert(in.left_to_read() > 0);

		this->require_write_space_inplace(in.left_to_read());

		u8* src = in.read;
		u8* dest = out.write;

		while (src != in.write) {
			*dest++ = *src++ + 1;
		}

		out.write = dest;
		in.read = src;

		return next->pump();
	}
};

struct UnpackFilter : Filter {
	int state;
	u64 flags;
	u8* dest;
	u32 i;

	UnpackFilter(Sink* next_init = 0) : Filter(next_init), state(0) {}

	void pump() {
		switch (state) {
			case 0:
				while (in.left_to_read() < 8) { state = 0; return; }
			
				this->dest = this->require_write_space(8);

				this->flags = tl::read_le<u64>(in.read);
				in.read += 8;

				for (this->i = 0; this->i < 64; ++this->i) {
					if ((this->flags >> this->i) & 1) {
						case 1: while (in.left_to_read() < 1) { state = 1; return; }
						*this->dest++ = *in.read++;
					} else {
						*this->dest++ = 0;
					}
				}

				this->next->in.write = this->dest;
		}
	}
};

struct Printer : Sink {
	void pump() {
		u8* src = in.read;

		if (in.write - src < 5)
			return;

		while (src != in.write) {
			printf("%d ", *src++);
		}

		in.read = src;

		printf("\n");
	}
};

void test_io() {
	Printer pr;
	AddOnFilter flt2(&pr);
	UnpackFilter flt(&flt2);

	u8 test_data[] = { 0, 0, 0, 1, 0, 0, 0, 0, 5 };

	flt.in.read = test_data;
	flt.in.write = flt.in.end = test_data + sizeof(test_data);
	flt.in.allow_overwrite = true;

	flt.pump();

	/*
	while (flt.in.read != test_data + 9) {
	}
	*/
}

void test_io2() {
	Printer pr;
	AddOnFilter flt(&pr);

	u8 test_data[256];

	memset(test_data, 1, 256);

	flt.in.read = test_data;
	flt.in.write = flt.in.end = test_data + 4;
	flt.in.allow_overwrite = true;

	while (flt.in.read != test_data + 256) {
		flt.pump();

		if (flt.in.write < test_data + 256) {
			flt.in.write += 4;
		}
	}
}