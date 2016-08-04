#ifndef CHANNEL_HPP
#define CHANNEL_HPP 1

#include "tl/cstdint.h"
#include <cstddef>
#include <algorithm>

// { state }
// { id: 

struct packet {
	u32 start;
	u16 length;
	u16 reserved;
	u8 data[];
};

static_assert(sizeof(packet) == 4*1 + 2*2, "Expected no padding in packet struct");

struct channel {
	// TODO: Remember old buffers that have not been
	// acknowledged
	u8 buffer[2048];
	u8* next;
	u8* end;

	channel()
		: next(buffer)
		, end(buffer + 1500) {
		
	}

	// TODO: Make the channel into a writable buffer
	void send(u8 const* msg, std::size_t len) {
		while (len) {
			std::size_t count = std::min(len, (std::size_t)(this->end - this->next));
			memcpy(this->next, msg, count);
			this->next += count;
			len -= count;

			if (this->next == this->end) {
				// TODO: Flush
				this->next = buffer;
				// Reserve space for packet header
				this->next += sizeof(packet);
			}
		}
	}
};

#endif // CHANNEL_HPP
