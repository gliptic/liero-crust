#ifndef SFX_HPP
#define SFX_HPP 1

#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/string.hpp>
#include <memory>

namespace sfx {

struct Guid {
	u32 data1;
	u16 data2;
	u16 data3;
	u8 data4[8];
};

struct Device {
	tl::String name;
	u32 max_out_channels;
	u32 default_sample_rate;

#if TL_WINDOWS
	Guid guid;
	bool guid_is_null;
#else
# error "Unsupported platform"
#endif

	Device(tl::StringSlice name_init)
		: name(name_init) {
	}

	Device(Device const&) = delete;
	Device& operator=(Device const&) = delete;

	Device(Device&&) = default;
	Device& operator=(Device&&) = default;
};

struct Stream;
struct Context;

typedef void (*FillFunc)(Stream& str, u32 start, u32 frames);

struct Stream {

	Context* owner;
	i16* buffer;
	void* ud;
	u32 sample_rate;
	u32 successful_calls;
	i32 output_underflow_count;
	u32 stream_pos;
	FillFunc fill;
	bool output_is_running;

	u32 start();
	u32 stop();

	~Stream();
};


struct Context {
	tl::Vec<Device> devices;
	double seconds_per_tick;
	bool com_was_initialized;
	bool timer_initialized;

	Context(Context const&) = delete;
	Context& operator=(Context const&) = delete;

	Context(Context&&) = default;
	Context& operator=(Context&&) = default;

	static Context create();

	std::unique_ptr<Stream> open(FillFunc fill, void* user_data);
	u64 get_time();

private:
	Context()
		: com_was_initialized(false)
		, timer_initialized(false) {
	}
};

extern long minPlayGap;

}


#endif // SFX_HPP
