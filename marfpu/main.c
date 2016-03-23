#include "gfx/window.h"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include "tl/std.h"

typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

void testcpp();

int main() {

	testcpp();

	gfx_common_window* win = gfx_window_create();
	gfx_window_set_mode(win, 1024, 768, 0);
	gfx_window_set_visible(win, 1);

	PFNWGLSWAPINTERVALEXTPROC sw = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	sw(1);

	while (1) {
		gfx_window_clear(win);

		glColor3d(1, 0, 0);

		//glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);

		float tringles[2 * 3] = {
			50.f, 50.f,
			100.f, 50.f,
			100.f, 100.f

		};

		glVertexPointer(2, GL_FLOAT, 0, tringles);

		glDrawArrays(GL_TRIANGLES, 0, 3);

		glDisableClientState(GL_VERTEX_ARRAY);
		//glDisableClientState(GL_COLOR_ARRAY);

		do {
			if (!gfx_window_update(win)) { goto end; }
			win->events.size = 0;

		} while (!gfx_window_end_drawing(win));
	}
end:

	gfx_window_destroy(win);

	return 0;
}