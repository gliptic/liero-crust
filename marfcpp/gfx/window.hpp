#ifndef UUID_301BAD699D8046FC06E5D5A9BFB45EA1
#define UUID_301BAD699D8046FC06E5D5A9BFB45EA1

#include "tl/std.h"
#include "tl/cstdint.h"
#include "tl/vector.hpp"
#include "gfx.hpp"
#include "buttons.hpp"

namespace gfx {

struct common_window;

GFX_API common_window* window_create();
//GFX_API void gfx_window_destroy(gfx_common_window* self);

/*
GFX_API int gfx_window_set_mode(gfx_common_window* self, u32 width, u32 height, int fullscreen);
GFX_API int gfx_window_set_visible(gfx_common_window* self, int state);
GFX_API int gfx_window_update(gfx_common_window* self);
GFX_API int gfx_window_end_drawing(gfx_common_window* self);
GFX_API int gfx_window_clear(gfx_common_window* self);
*/

enum
{
	ev_button = 0
};

typedef struct event
{
	char ev;
	char down;
	union gfx_event_u
	{
		struct { int id; } button;
	} d;
} event;


struct common_window {

	common_window() {
		memset(this, 0, sizeof(common_window));
		this->refresh_rate = 60;
		this->bpp = 32;
		this->min_swap_interval = 0xffffffffffffffffull;
	}

	~common_window();

	int set_mode(u32 width, u32 height, int fullscreen);
	int set_visible(int state);
	int update();
	int end_drawing();
	int clear();

	int fsaa;
	unsigned int width;
	unsigned int height;
	int refresh_rate;
	int bpp;
	char fullscreen;
	int mouseX, mouseY;
	char buttonState[numButtons + 1];
	tl::vector<event> events;
	u64 swap_delay;
	u64 swaps, prev_swap_end, swap_end;
	u64 draw_delay;
	u64 min_swap_interval;
	u64 frame_intervals[4], frame_interval_n;
};

}

#endif // UUID_301BAD699D8046FC06E5D5A9BFB45EA1
