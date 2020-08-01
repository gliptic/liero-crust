#include "test_cellphase.hpp"

//#include "test_cellphase/state1.hpp"

#define TICKS_IN_SECOND (10000000)

#include "test_cellphase/state1.hpp"
#include "test_cellphase/state2.hpp"
#include "test_cellphase/state_sll.hpp"

namespace test_cellphase {


void test_cellphase() {

	bool vsync = false;
	u32 repeat = 1;
	bool graphics = true;

	int scrw = 1024, scrh = 768;

	gfx::CommonWindow win;
	win.fsaa = 0;
	win.set_mode(scrw, scrh, 0);
	win.set_visible(1);
	
	gfx::Texture tex(512, 512);

	u32 canvasw = 504, canvash = 350;
	tl::Image img(canvasw, canvash, 4);

	memset(img.data(), 0, img.size());

	wglSwapIntervalEXT((int)vsync);

	gfx::GeomBuffer buf(win.white_texture);

	auto root = tl::FsNode(tl::FsNode::from_path("TC/lierov133"_S));

	liero::Mod mod(root);
	StateSll state1;
	state1.start();
	
	{
		auto src = (root / "2xkross2.lev"_S).try_get_source();
		state1.level = liero::Level(src, mod.pal, mod.ref());
	}

	tl::LcgPair test_rand(1, 1);

	u64 prev_frame = tl_get_ticks();
#define TICKS_PER_FRAME (TICKS_IN_SECOND / 60)
#define TICKS_IN_MS (TICKS_IN_SECOND / 1000)

	while (true) {

		//if (gui.mode == gui::LieroGui::Game) {
		//liero::ControlState c;
		//c.set(liero::WcLeft, win.button_state[kbA] != 0);
		//c.set(liero::WcRight, win.button_state[kbO] != 0);
		//c.set(liero::WcUp, win.button_state[kbU] != 0);
		//c.set(liero::WcDown, win.button_state[kbE] != 0);
		//c.set(liero::WcFire, win.button_state[kbT] != 0);
		//c.set(liero::WcJump, win.button_state[kbS] != 0);
		//c.set(liero::WcChange, win.button_state[kbD] != 0);
		state1.process();

#if 0
		if ((state.current_time % 60) == 0) {
			col_tests += transient_state.col_tests;
			col2_tests += transient_state.col2_tests;
			col_mask_tests += transient_state.col_mask_tests;
			printf("%d/%d/%d\n", col_tests, col2_tests, col_mask_tests);
			printf("%d vs %d\n", state.worms.of_index(0).health, state.worms.of_index(1).health);
		}
#endif
		//}

		auto render = [&] {
			{
				liero::DrawTarget target(img.slice(), mod.pal);

				state1.draw(target);
				tl::LcgPair rand(1, 1);

#if RANDOM_VIS
				u32 w = 128;
				for (i32 y = 0; y < w; ++y) {
					for (i32 x = 0; x < w; ++x) {

						{
							u32 i = rand.get_i32(256);
							u32 col = (i << 16) | (i << 8) | (i);
							target.image.set_pixel32(tl::VectorI2(x, y), col);
						}

						{
							auto r = Token(0, y * w + x).get_seed(0x527731fc);
							u32 i = r.get_i32(256);
							u32 col = (i << 16) | (i << 8) | (i);
							target.image.set_pixel32(tl::VectorI2(x + 130, y), col);

						}
					}
				}
				
				for (i32 i = 0; i < 128; ++i) {
					auto r2 = Token(0, i).get_seed(0x1eaa09e6);
					auto v = r2.get_vectori2(128);

					i32 br = i * 256 / (128);

					target.image.set_pixel32(tl::VectorI2(130 + 64 + v.x, 130 + 64 + v.y), br * 0x010101 | 0xff000000);
				}
#endif

			}

			tex.upload_subimage(img.slice());

#if !BONK_USE_GL2
			glDisable(GL_BLEND);
#endif

			win.textured.use();
			buf.color(1, 1, 1, 1);
			buf.quads();

			buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, canvasw / 512.f, canvash / 512.f, tex.id);
			buf.clear();

			win.textured_blended.use();

			

			//buf.color(1, 1, 1, 1);
			//font.draw_text(buf, "/\\Test lol"_S, tl::VectorF2(50.f, 50.f), 5.f);
			buf.clear();
		};

		win.clear();
#if GFX_PREDICT_VSYNC
		u64 draw_delay = tl::timer(render);
#else	
		render();
#endif

#if !BONK_USE_GL2
		glEnable(GL_BLEND);
#endif

		//do {
		if (!win.update()) { return; }

		for (auto& ev : win.events) {
			if (ev.ev == gfx::ev_button && ev.down) {
				if (ev.d.button.id == kbEscape) {
					//gui.mode = gui::LieroGui::Main;
				}
			}
		}

#if GFX_PREDICT_VSYNC
		win.draw_delay += draw_delay;
#endif
		win.events.clear();

		//} while (!win.end_drawing());

		if (!vsync) {
			while (true) {
				u64 cur = tl_get_ticks();

				if (cur >= prev_frame + TICKS_PER_FRAME) {
					do
						prev_frame += TICKS_PER_FRAME;
					while (cur >= prev_frame + TICKS_PER_FRAME);
					break;
				}

				i32 sleep = i64(prev_frame + TICKS_PER_FRAME - TICKS_IN_MS * 2 - cur) / TICKS_IN_MS;

				if (sleep > 0) {
					tl::win::Sleep(sleep);
				}
				else {
					tl::win::Sleep(0);
				}
			}
		}
		win.end_drawing();
	}

	//printf("%08x\n", state.rand.s[0] ^ state.worms.of_index(0).pos.x.raw());
}

}
