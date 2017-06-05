#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/texture.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include "sfx/sfx.hpp"
#include "sfx/mixer.hpp"
#include "gfx/sprite_set.hpp"
#include "bmp.hpp"
#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/rand.h>
#include <tl/io/stream.hpp>
//#include <tl/rectpack.hpp>
#include <memory>

#include "lierosim/state.hpp"
#include "liero/viewport.hpp"
#include "liero/font.hpp"
#include "ai.hpp"
#include "liero/gui.hpp"

void mixer_fill(sfx::Stream& str, u32 /*start*/, u32 frames) {
	sfx::Mixer* mixer = (sfx::Mixer *)str.ud;

	mixer->mix(str.buffer, frames);
}

void mixer_play_sound(liero::ModRef& mod, i16 sound_index, liero::TransientState& transient_state) {

	//return; // TEMP

	if (sound_index < 0)
		return;

	sfx::Mixer* mixer = (sfx::Mixer *)transient_state.sound_user_data;
	mixer->play(&mod.sounds[sound_index], mixer->now(), 1, 1);
}

namespace liero {
void do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state);
}

bool vsync = true;
u32 repeat = 1;
bool graphics = true;
#define RUN_AI 0

#define TICKS_IN_SECOND (10000000)

struct Sampler {
	enum Type {
		Clear,
		BeforeDraw,
		AfterFirstDraw,
		BeforeSwap,

		Max
	};

	struct Sample {
		u64 v[Max];
	};

	static int const MaxSamples = 256;

	Sample samples[MaxSamples];
	u64 mins[Max];
	int count = 0;
	int last_sleeps = 0;

	Sampler() {
		memset(mins, 0, sizeof(mins));
	}

	void sample(Type type) {
		auto now = tl_get_ticks();
		samples[count & (MaxSamples - 1)].v[(int)type] = now;

		if (mins[type] >= TICKS_IN_SECOND / 60 / 2) {
			u64 target = now + mins[type] * 15 / 16;
			last_sleeps = 0;
			do {
				DWORD sl = DWORD((target - now) / (TICKS_IN_SECOND / 1000));
				if (sl < 1) sl = 1;
				Sleep(sl);
				last_sleeps += sl;
				now = tl_get_ticks();
			} while (now < target);
		}
	}

	void wait(Type type) {
		auto cur_time = tl_get_ticks();


	}

	void done_frame() {
		++count;

		if ((count & (MaxSamples - 1)) == 0) {
			u64 sums[Max] = {};
			u64 m[Max] = {
				0xffffffffffffffff,
				0xffffffffffffffff, 
				0xffffffffffffffff, 
				0xffffffffffffffff
			};

			for (u32 i = 0; i < MaxSamples; ++i) {
				u64 d0 = samples[i].v[1] - samples[i].v[0];
				u64 d1 = samples[i].v[2] - samples[i].v[1];
				u64 d2 = samples[i].v[3] - samples[i].v[2];
				m[0] = tl::min(m[0], d0);
				m[1] = tl::min(m[1], d1);
				m[2] = tl::min(m[2], d2);
				sums[0] += d0;
				sums[1] += d1;
				sums[2] += d2;
				if (i < MaxSamples - 1) {
					u64 d3 = samples[i + 1].v[0] - samples[i].v[3];
					sums[3] += d3;
					m[3] = tl::min(m[3], d3);
				}
			}

			mins[0] = m[0];
			mins[1] = m[1];
			mins[2] = m[2];
			mins[3] = m[3];

			printf("%.4f %.4f %.4f %.4f\n",
				(i64)mins[0] / (f64)TICKS_IN_SECOND,
				(i64)mins[1] / (f64)TICKS_IN_SECOND,
				(i64)mins[2] / (f64)TICKS_IN_SECOND,
				(i64)mins[3] / (f64)TICKS_IN_SECOND);
			printf("%.4f %.4f %.4f %.4f %d\n",
				(i64)sums[0] / (MaxSamples * (f64)TICKS_IN_SECOND),
				(i64)sums[1] / (MaxSamples * (f64)TICKS_IN_SECOND),
				(i64)sums[2] / (MaxSamples * (f64)TICKS_IN_SECOND),
				(i64)sums[3] / ((MaxSamples - 1) * (f64)TICKS_IN_SECOND),
				last_sleeps);
		}
	}


};

void test_gfx() {

	int scrw = 1024, scrh = 768;
	//int scrw = 504 * 2, scrh = 350 * 2;

	gfx::CommonWindow win;
	win.fsaa = 0;
	win.set_mode(scrw, scrh, 0);
	win.set_visible(1);

	auto ctx = sfx::Context::create();

	sfx::Mixer mixer;
	auto str = ctx.open(mixer_fill, &mixer);
	
	gfx::Texture tex(512, 512);

#if SDF
	gfx::DefaultProgram sdf_program;

	gfx::Shader sdf_vs_shader(gfx::Shader::Vertex,
R"=(#version 110
uniform mat2 transform;
uniform vec2 translation;
attribute vec2 position;
attribute vec2 texcoord;
attribute vec4 color;

varying vec2 fragTexcoord;
varying vec4 fragColor;

void main()
{
	gl_Position = vec4((transform * position) + translation, 0.0, 1.0);
	fragTexcoord = texcoord;
	fragColor = color;
})=");

	gfx::Shader sdf_shader(gfx::Shader::Fragment, 
R"=(#version 110

uniform sampler2D texture;
varying vec2 fragTexcoord;
varying vec4 fragColor;

vec4 cubic(float v)
{
	vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4(x, y, z, w);
}

vec4 filter(sampler2D texture, vec2 texcoord, vec2 texscale)
{
	float fx = fract(texcoord.x);
	float fy = fract(texcoord.y);
	texcoord.x -= fx;
	texcoord.y -= fy;

	vec4 xcubic = cubic(fx);
	vec4 ycubic = cubic(fy);

	vec4 c = vec4(texcoord.x - 0.5, texcoord.x + 1.5, texcoord.y - 0.5, texcoord.y + 1.5);
	vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	vec4 sample0 = texture2D(texture, vec2(offset.x, offset.z) * texscale);
	vec4 sample1 = texture2D(texture, vec2(offset.y, offset.z) * texscale);
	vec4 sample2 = texture2D(texture, vec2(offset.x, offset.w) * texscale);
	vec4 sample3 = texture2D(texture, vec2(offset.y, offset.w) * texscale);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
		mix(sample3, sample2, sx),
		mix(sample1, sample0, sx), sy);
}

void main()
{
	float distance = texture2D(texture, fragTexcoord).r - 0.5;
	//float distance = filter(texture, fragTexcoord, vec2(1.0/32.0, 1.0/32.0)).r - 0.5;
	float alpha = clamp(distance / fwidth(distance) + 0.5, 0.0, 1.0);

	gl_FragColor = vec4(fragColor.rgb, alpha);
})=");

	sdf_program.init(std::move(sdf_vs_shader), std::move(sdf_shader), win.width, win.height);

	gfx::Texture sdf(32, 32, true);

	{
		tl::Image img(32, 32, 1);

		auto lines = img.lines_range();
		tl::ImageSlice::PixelsRange<u8> pixels;
		u32 y = 0;
		while (lines.next(pixels)) {
			u8* pix;
			u32 x = 0;
			while (pixels.next(pix)) {

				double dist;

				double dx = x / 32.0;
				double dy = y / 32.0;

				{
					double dx1 = dx - 0.5;
					double dy1 = dy - 0.5;
					double dist1 = 10.0/32.0 - sqrt(dx1*dx1 + dy1*dy1);

					double dx2 = dx - 10.0/32.0;
					double dy2 = dy - 20.0/32.0;
					double dist2 = sqrt(dx2*dx2 + dy2*dy2) - 5.0/32.0;

					double dx3 = dx - 15.0/32.0;
					double dy3 = dy - 10.0/32.0;
					double dist3 = sqrt(dx3*dx3 + dy3*dy3) - 1.5/32.0;

					double dx4 = dx - 0.1;
					double dy4 = dy - 0.1;
					double dist4 = 0.3/32.0 - sqrt(dx4*dx4 + dy4*dy4);

					dist = min(min(max(dist1, dist4), dist2), dist3);
				}

				double df = (dist / (2.0/32.0) + 0.5) * 256.0;

				int idf = int(df);
				if (idf < 0) idf = 0;
				else if (idf > 255) idf = 255;

				*pix = idf;

				++x;
			}
			++y;
		}

		sdf.upload_subimage(img.slice());
	}
#endif

	u32 canvasw = 504, canvash = 350;
	tl::Image img(canvasw, canvash, 4);
	
	memset(img.data(), 0, img.size());

	wglSwapIntervalEXT((int)vsync);

	gfx::GeomBuffer buf(win.white_texture);

	auto root = tl::FsNode(tl::FsNode::from_path("TC/lierov133"_S));

	liero::Mod mod(root);
	liero::State state(mod.ref());
	Sampler sampler;

	// Font test
#if 1
	gfx::SpriteSetAcc spriteSet;
	Font font;

	{
		auto slice = mod.font_sprites.slice();
		u32 count = slice.height() / 8;

		for (u32 i = 0; i < count; ++i) {
			u32 w, h = 8;
			u32 y = i * 8;
			for (w = 0; w < 8; ++w) {
				if (slice.unsafe_pixel8(w, y) == 1) {
					break;
				}
			}

			font.alloc_char(spriteSet, slice.crop(tl::RectU(0, y, w, y + h)));
		}
	}
#endif

	{
		auto src = (root / "2xkross2.lev"_S).try_get_source();
		state.level = liero::Level(src, mod.pal, state.mod);
	}

	{
		liero::Worm* w = state.worms.new_object();
		new (w, tl::non_null()) liero::Worm();
		for (u32 i = 0; i < liero::Worm::SelectableWeapons; ++i) {
			w->weapons[i].ty_idx = i;
		}
		//w->pos = liero::Vector2(70, 70);
		w->pos = liero::find_spawn(state.level, state.rand).cast<liero::Scalar>();
		w->spawn(state);
	}

	{
		liero::Worm* w = state.worms.new_object();
		new (w, tl::non_null()) liero::Worm();
		for (u32 i = 0; i < liero::Worm::SelectableWeapons; ++i) {
			w->weapons[i].ty_idx = i;
		}
		w->pos = liero::Vector2(180, 70);
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

	str->start();

	//u32 col_tests = 0, col2_tests = 0, col_mask_tests = 0;

	liero::Ai ai(mod.ref());

	u64 total_ai_time = 0;
	liero::TransientState transient_state;

	tl::LcgPair test_rand(1, 1);
	
	// GUI
	gui::Context3 gui_ctx(font);

	//while (state.current_time < 60*60*repeat) {
	while (true) {
		{
			liero::ControlState c;
			c.set(liero::WcLeft, win.button_state[kbLeft] != 0);
			c.set(liero::WcRight, win.button_state[kbRight] != 0);
			c.set(liero::WcUp, win.button_state[kbUp] != 0);
			c.set(liero::WcDown, win.button_state[kbDown] != 0);
			c.set(liero::WcFire, win.button_state[kbT] != 0);
			c.set(liero::WcJump, win.button_state[kbS] != 0);
			c.set(liero::WcChange, win.button_state[kbD] != 0);

			total_ai_time += tl::timer([&] {
				for (u32 i = 0; i < repeat; ++i) {

					transient_state.init(state.worms.size(), mixer_play_sound, &mixer);
					transient_state.graphics = graphics;
					// Process
					transient_state.worm_state[0].input = liero::WormInput::from_keys(c);

#if RUN_AI
					ai.do_ai(state, state.worms.of_index(1), 1, transient_state.worm_state[1]);
#elif SPAM
					{
						using namespace liero;

						u32 phase = state.current_time & 15;

						WormInput in;
						if (phase == 0) {
							in = WormInput::change(WormInput::Move::Left, WormInput::Aim::None);
						} else {
							in = WormInput::combo(false, true, WormInput::Move::None, WormInput::Aim::None);
						}

						transient_state.worm_state[1].input = in;
					}
#elif SPAM2
					liero::fire(
						state.mod.get_weapon_type(state.current_time % 40), state, transient_state,
						liero::Scalar((i32)state.current_time * 100),
						liero::Vector2(),
						liero::Vector2(liero::Scalar(i32(state.current_time % 500)), liero::Scalar(100)),
						-1);
#endif

					state.update(transient_state);
				}
			});

#if 0
			if ((state.current_time % 60) == 0) {
				col_tests += transient_state.col_tests;
				col2_tests += transient_state.col2_tests;
				col_mask_tests += transient_state.col_mask_tests;
				printf("%d/%d/%d\n", col_tests, col2_tests, col_mask_tests);
				printf("%d vs %d\n", state.worms.of_index(0).health, state.worms.of_index(1).health);
			}
#endif
		}

		auto render = [&] {
			{
				liero::DrawTarget target(img.slice(), mod.pal);

				for (auto& vp : viewports) {
					vp.draw(state, target, transient_state);
				}
#if 0
				for (int i = 0; i < 128; ++i) {
					f64 radius = 90.0;
					u32 offset = 200;

					tl::VectorI2 sc;

					/*
					sc = tl::sincos_fixed(i << 16);
					target.image.unsafe_pixel32(200 + i32(sc.x * radius / 65536.0), 200 + i32(sc.y * radius / 65536.0)) = 0xffffffff;
					*/

					sc = tl::sincos_fixed2(i << 16);
					target.image.unsafe_pixel32(200 + i32(sc.x * radius / 65536.0), 200 + i32(sc.y * radius / 65536.0)) = 0xffff7f7f;

					auto sc2 = tl::sincos(i * (tl::pi2 / 128.0));
					target.image.unsafe_pixel32(200 + i32(sc2.x * radius), 200 + i32(sc2.y * radius)) = 0xff7f7fff;
				}
#endif
			}

			sampler.sample(Sampler::BeforeDraw);
			tex.upload_subimage(img.slice());
			
#if !BONK_USE_GL2
			glDisable(GL_BLEND);
#endif

			win.textured.use();
			buf.color(1, 1, 1, 1);
			buf.quads();

			buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, canvasw / 512.f, canvash / 512.f, tex.id);
			buf.clear();

			sampler.sample(Sampler::AfterFirstDraw);

			win.textured_blended.use();

			// GUI
			{
				gui::LieroGui gui;
				gui.run(gui_ctx);
				gui_ctx.render2(buf);
				
			}
			
			//buf.color(1, 1, 1, 1);
			//font.draw_text(buf, "/\\Test lol"_S, tl::VectorF2(50.f, 50.f), 5.f);
			buf.clear();
		};

		sampler.sample(Sampler::Clear);
		win.clear();
#if GFX_PREDICT_VSYNC
		u64 draw_delay = tl::timer(render);
#else	
		render();
#endif

#if SDF
		sdf_program.use();

		buf.quads();
		buf.tex_rect(0.f, 0.f, 512.0, 512.0, 0.f, 0.f, 1.f, 1.f, sdf.id);
		//buf.tex_rect(0.f, 0.f, 512.0, 512.0, 0.f, 0.f, 32.0f, 32.0f, sdf.id);
			
		buf.clear();
#endif

#if !BONK_USE_GL2
		glEnable(GL_BLEND);
#endif

		//do {
			if (!win.update()) { return; }

			for (auto& ev : win.events) {
				TL_UNUSED(ev);
				//
			}

#if GFX_PREDICT_VSYNC
			win.draw_delay += draw_delay;
#endif
			win.events.clear();

		//} while (!win.end_drawing());
		sampler.sample(Sampler::BeforeSwap);
		win.end_drawing();

		sampler.done_frame();
	}

	printf("%08x\n", state.rand.s[0] ^ state.worms.of_index(0).pos.x.raw());
}
