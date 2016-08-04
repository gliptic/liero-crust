#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/texture.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include "bmp.hpp"
#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/rand.h>
#include <tl/stream.hpp>
#include <memory>

typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include "lierosim/state.hpp"
#include "liero/viewport.hpp"

void test_gfx() {

	int scrw = 1024, scrh = 768;

	gfx::CommonWindow win;
	win.set_mode(scrw, scrh, 0);
	win.set_visible(1);

	gfx::Texture tex(512, 512);

	u32 canvasw = 504, canvash = 350;
	tl::Image img(canvasw, canvash, 4);
	
	memset(img.data(), 0, img.size());

	PFNWGLSWAPINTERVALEXTPROC sw = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	sw(1);

	gfx::GeomBuffer buf;

	liero::Mod mod;
	liero::State state(mod);
	{
		auto src = tl::source::from_file("C:\\Users\\glip\\Documents\\cpp\\marfpu\\_build\\TC\\lierov133winxp\\2xkross2.lev");
		state.level = liero::Level(src, mod.pal);
	}

	state.mod.weapon_types[0].fire(state, liero::Scalar(0.3), liero::Vector2(), liero::Vector2(100, 100));

	{
		liero::Worm* w = state.worms.new_object();
		new (w) liero::Worm();
		w->weapons[1].ty_idx = 1; // float mine
		w->weapons[2].ty_idx = 2; // fan
		w->pos = liero::Vector2(70, 70);
		w->spawn(state);
	}

	liero::Viewport viewports[] =
	{
#if 0
		liero::Viewport(tl::RectU(0, 0, 158, 158)),
		liero::Viewport(tl::RectU(160, 0, 320, 158))
#else
		liero::Viewport(tl::RectU(0, 0, 504, 350))
#endif
	};

	while (true) {
		win.clear();

		{
			// Process

			state.update(win);
		}

		{
			liero::DrawTarget target(img.slice(), mod.pal);

			for (auto& vp : viewports) {
				vp.draw(state, target);
			}
		}

		tex.upload_subimage(img.slice());
		
		glDisable(GL_BLEND);
		buf.color(1, 1, 1, 1);
		buf.quads();

		buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, canvasw / 512.f, canvash / 512.f, tex.id);

		TL_TIME(draw_delay, {
			buf.clear();
		});
		glEnable(GL_BLEND);

		do {
			if (!win.update()) { return; }

			for (auto& ev : win.events) {
				TL_UNUSED(ev);
				//
			}

			win.draw_delay += draw_delay;
			win.events.clear();

		} while (!win.end_drawing());

	}
}
