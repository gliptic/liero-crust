
#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/texture.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/rand.h>
#include <tl/rand.hpp>
#include <tl/io/stream.hpp>
#include <memory>
#define _USE_MATH_DEFINES
#include <math.h>
#include <tl/approxmath/am.hpp>

//#include "lierosim/state.hpp"
//#include "liero/viewport.hpp"
#include <tl/windows/miniwindows.h>
//#include <liero-sim/config.hpp>
#include <tl/cstdint.h>
#include <tl/vector.hpp>

#include "lierosim/object_list.hpp"
#include "test_ships/test_ships.hpp"
#include "test_ships/mcts.hpp"

#define TICKS_IN_SECOND (10000000)

namespace test_ships {

	void test_ships() {

		bool vsync = false;
		u32 repeat = 1;
		bool graphics = true;

		int scrw = 1024, scrh = 768;

		auto d = dir(0.0);
		d = dir(M_PI / 2.0);
		d = dir(M_PI);
		d = dir(M_PI * 1.5);

		gfx::CommonWindow win;
		win.fsaa = 4;
		win.set_mode(scrw, scrh, 0);
		win.set_visible(1);

		//gfx::Texture tex(512, 512);

		//u32 canvasw = 504, canvash = 350;
		//tl::Image img(canvasw, canvash, 4);

		//memset(img.data(), 0, img.size());

		wglSwapIntervalEXT((int)vsync);

		gfx::GeomBuffer buf(win.white_texture);

		tl::LcgPair test_rand(1, 1);

		auto center = tl::VectorF2((f32)scrw / 2, (f32)scrh / 2);
		//auto bot = SillyBot();
		auto bot = PredBot();

		State state;
		for (u32 i = 0; i < 2; ++i) {
			state.tanks[i].aim = i * M_PI;
			state.tanks[i].rot = i * M_PI;
			state.tanks[i].pos = i == 0 ? center - center * 0.3 : center + center * 0.3;
			state.tanks[i].vel = 0;
			state.tanks[i].hp = 20;
			state.tanks[i].cooldown = 0;
		}

		bool fire = false;

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
			//state1.process();
			State::TransientState st;

			auto mouseVec = tl::VectorF2(win.mouseX, win.mouseY) - state.tanks[0].pos;
			auto aiming = dir(state.tanks[0].aim);
			auto diff = tl::unrotate(mouseVec, aiming);

			st.tank_controls[0] = State::Controls(0);
			st.tank_controls[1] = bot.move(state);
			if (win.button_state[kbG]) {
				st.tank_controls[0] |= State::c_forward;
			}
			if (win.button_state[kbS]) {
				st.tank_controls[0] |= State::c_back;
			}
			if (win.button_state[kbD]) {
				st.tank_controls[0] |= State::c_left;
			}
			if (win.button_state[kbT]) {
				st.tank_controls[0] |= State::c_right;
			}
			if (diff.y < 0.0) {
				st.tank_controls[0] |= State::c_aim_left;
			}
			if (diff.y > 0.0) {
				st.tank_controls[0] |= State::c_aim_right;
			}
			if (win.button_state[kbSpace]) {
				st.tank_controls[0] |= State::c_fire;
			}
			state.process(st);
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
					//liero::DrawTarget target(img.slice(), mod.pal);

					//state1.draw(target);
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

				//tex.upload_subimage(img.slice());

#if !BONK_USE_GL2
				glDisable(GL_BLEND);
#endif

				win.textured.use();
				buf.tris();

				auto aim_radius = 23.0;
				/*
				auto lr_trans = dir(M_PI / 4.0) * tank_radius;
				auto ll_trans = dir(M_PI / 2.0 + M_PI / 4.0) * tank_radius;
				auto ul_trans = dir(M_PI + M_PI / 4.0) * tank_radius;
				auto ur_trans = dir(M_PI * 1.5 + M_PI / 4.0) * tank_radius;
				*/
				auto lr_trans = tl::VectorF2(tank_w, tank_h);
				auto ll_trans = tl::VectorF2(-tank_w, tank_h);
				auto ul_trans = tl::VectorF2(-tank_w, -tank_h);
				auto ur_trans = tl::VectorF2(tank_w, -tank_h);

				auto lr_trans_aim = dir(M_PI / 15.0) * aim_radius;
				auto ll_trans_aim = dir(M_PI / 2.0) * 5.0;
				auto ul_trans_aim = dir(-M_PI / 2.0) * 5.0;
				auto ur_trans_aim = dir(-M_PI / 15.0) * aim_radius;


				for (u32 i = 0; i < 2; ++i) {
					auto& tank = state.tanks[i];
					auto heading = dir(tank.rot);
					auto aiming = dir(tank.aim);

					auto lr = tank.pos + tl::rotate(heading, lr_trans);
					auto ll = tank.pos + tl::rotate(heading, ll_trans);
					auto ul = tank.pos + tl::rotate(heading, ul_trans);
					auto ur = tank.pos + tl::rotate(heading, ur_trans);

					buf.color(0.6, 0.6, 0.65, 1);
					buf.tri_quad(ul, ur, lr, ll);

					auto lr_aim = tank.pos + tl::rotate(aiming, lr_trans_aim);
					auto ll_aim = tank.pos + tl::rotate(aiming, ll_trans_aim);
					auto ul_aim = tank.pos + tl::rotate(aiming, ul_trans_aim);
					auto ur_aim = tank.pos + tl::rotate(aiming, ur_trans_aim);

					buf.color(0.85, 0.8, 0.8, 1);
					buf.tri_quad(ul_aim, ur_aim, lr_aim, ll_aim);

					buf.color(1, 0, 0, 1);
					buf.tri_rect(tank.pos + tl::VectorF2(-10, 40), tank.pos + tl::VectorF2(-10 + 20.0 * tank.hp / 20.0, 45));
				}

				buf.color(1, 1, 0, 1);
				auto r = state.bullets.all();

				for (Bullet* b; (b = r.next()) != 0; ) {
					auto ul_cursor = tl::VectorF2(b->pos.x - 2.0, b->pos.y - 2.0);
					auto ur_cursor = tl::VectorF2(b->pos.x + 2.0, b->pos.y - 2.0);
					auto lr_cursor = tl::VectorF2(b->pos.x + 2.0, b->pos.y + 2.0);
					auto ll_cursor = tl::VectorF2(b->pos.x - 2.0, b->pos.y + 2.0);

					buf.tri(ul_cursor.x, ul_cursor.y, ur_cursor.x, ur_cursor.y, ll_cursor.x, ll_cursor.y);
					buf.tri(ur_cursor.x, ur_cursor.y, lr_cursor.x, lr_cursor.y, ll_cursor.x, ll_cursor.y);
				}

				{
					buf.color(1, 1, 1, 1);
					auto ul_cursor = tl::VectorF2(win.mouseX - 2.0, win.mouseY - 2.0);
					auto ur_cursor = tl::VectorF2(win.mouseX + 2.0, win.mouseY - 2.0);
					auto lr_cursor = tl::VectorF2(win.mouseX + 2.0, win.mouseY + 2.0);
					auto ll_cursor = tl::VectorF2(win.mouseX - 2.0, win.mouseY + 2.0);

					buf.tri(ul_cursor.x, ul_cursor.y, ur_cursor.x, ur_cursor.y, ll_cursor.x, ll_cursor.y);
					buf.tri(ur_cursor.x, ur_cursor.y, lr_cursor.x, lr_cursor.y, ll_cursor.x, ll_cursor.y);
				}

				//buf.tri(30, 30, 40, 30, 30, 40);

				//buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, canvasw / 512.f, canvash / 512.f, tex.id);
				buf.clear();

				//win.textured_blended.use();



				//buf.color(1, 1, 1, 1);
				//font.draw_text(buf, "/\\Test lol"_S, tl::VectorF2(50.f, 50.f), 5.f);
				//buf.clear();
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
				if (ev.ev == gfx::ev_button) {
					if (ev.d.button.id == msLeft) {
						fire = ev.down;
					}
					if (ev.down && ev.d.button.id == kbEscape) {
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
