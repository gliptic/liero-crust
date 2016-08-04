#ifndef UUID_301BAD699D8046FC06E5D5A9BFB45EA1
#define UUID_301BAD699D8046FC06E5D5A9BFB45EA1

#include "tl/std.h"
#include "tl/cstdint.h"
#include "tl/vec.hpp"
#include "gfx.hpp"
#include "buttons.hpp"

namespace gfx {

struct CommonWindow;

enum {
	ev_button = 0
};

struct Event {
	char ev;
	char down;
	union gfx_event_u {
		struct { int id; } button;
	} d;
};


struct CommonWindow {

	CommonWindow();
	~CommonWindow();

	int set_mode(u32 width, u32 height, bool fullscreen);
	int set_visible(bool state);
	int update();
	int end_drawing();
	int clear();

	int fsaa;
	unsigned int width;
	unsigned int height;
	int refresh_rate;
	int bpp;
	bool fullscreen;
	int mouseX, mouseY;
	char button_state[numButtons + 1];
	tl::Vec<Event> events;
	u64 swap_delay;
	u64 swaps, prev_swap_end, swap_end;
	u64 draw_delay;
	u64 min_swap_interval;
	u64 frame_intervals[4], frame_interval_n;

#if TL_WINDOWS
	
	void* hwnd;
	void* hdc;
	bool closed;
	bool iconified;
	
	int swapMouse;
	u32 fsaaLevels;
#endif
};

}

#endif // UUID_301BAD699D8046FC06E5D5A9BFB45EA1
