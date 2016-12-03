#ifndef UUID_301BAD699D8046FC06E5D5A9BFB45EA1
#define UUID_301BAD699D8046FC06E5D5A9BFB45EA1

#include "gfx.hpp"
#include <tl/std.h>
#include <tl/cstdint.h>
#include <tl/vec.hpp>
#include "buttons.hpp"
#include "texture.hpp"
#if BONK_USE_GL2
#include "shader.hpp"
#endif

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

#if BONK_USE_GL2

struct DefaultProgram : Program {
	Shader vs, fs;
	Uniform transform_uni, translation_uni;
	Uniform texture;

	void init(Shader vs_init, Shader fs_init, u32 width, u32 height);
};
#endif

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
	u8 button_state[numButtons + 1];
	tl::Vec<Event> events;

#if GFX_PREDICT_VSYNC
	u64 swap_delay;
	u64 swaps, prev_swap_end, swap_end;
	u64 draw_delay;
	u32 frame_intervals[4], frame_interval_n;
	u32 min_swap_interval;
#endif

#if BONK_USE_GL2
	DefaultProgram textured;
	Texture white_texture;
#endif

#if TL_WINDOWS
	void* hwnd;
	void* hdc;
	
	u32 fsaaLevels;

	bool swapMouse;
	bool closed;
	bool iconified;
#endif
};

inline void check_gl() {
	GLenum err = glGetError();
	if (err != 0) {
		printf("GL error: %x\n", err);
		abort();
	}
}

}

#endif // UUID_301BAD699D8046FC06E5D5A9BFB45EA1
