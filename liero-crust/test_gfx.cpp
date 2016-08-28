#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/texture.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include "sfx/sfx.hpp"
#include "sfx/mixer.hpp"
#include "bmp.hpp"
#include <tl/std.h>
#include <tl/vec.hpp>
#include <tl/rand.h>
#include <tl/io/stream.hpp>
#include <memory>

#include "lierosim/state.hpp"
#include "liero/viewport.hpp"

void mixer_fill(sfx::Stream& str, u32 /*start*/, u32 frames) {
	sfx::Mixer* mixer = (sfx::Mixer *)str.ud;

	mixer->mix(str.buffer, frames);
}

void mixer_play_sound(liero::ModRef& mod, liero::TransientState& transient_state) {
	sfx::Mixer* mixer = (sfx::Mixer *)transient_state.sound_user_data;
	mixer->play(&mod.ninjarope_sound, mixer->now(), 1, 1);
}

void test_gfx() {

	int scrw = 1024, scrh = 768;

	gfx::CommonWindow win;
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

	wglSwapIntervalEXT(1);

	gfx::GeomBuffer buf;

	tl::FsNode root(new tl::FsNodeFilesystem("TC\\lierov133winxp"_S));

	liero::Mod mod(root);
	liero::State state(mod.ref());

	{
		auto src = (root / "2xkross2.lev"_S).try_get_source();
		state.level = liero::Level(src, mod.pal);
	}

	//fire(state.mod.weapon_types[0], state, liero::Scalar(0.3), liero::Vector2(), liero::Vector2(100, 100));

	{
		liero::Worm* w = state.worms.new_object();
		new (w, tl::non_null()) liero::Worm();
		for (u32 i = 0; i < liero::Worm::SelectableWeapons; ++i) {
			w->weapons[i].ty_idx = i;
		}
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

	str->start();

	while (true) {
		win.clear();

		liero::TransientState transient_state(state.worms.size(), mixer_play_sound, &mixer);

		{
			// Process
			
			auto& c = transient_state.worm_state[0].controls;
			c.set(liero::Worm::Left, win.button_state[kbLeft] != 0);
			c.set(liero::Worm::Right, win.button_state[kbRight] != 0);
			c.set(liero::Worm::Up, win.button_state[kbUp] != 0);
			c.set(liero::Worm::Down, win.button_state[kbDown] != 0);
			c.set(liero::Worm::Fire, win.button_state[kbS] != 0);
			c.set(liero::Worm::Jump, win.button_state[kbR] != 0);
			c.set(liero::Worm::Change, win.button_state[kbA] != 0);

			state.update(transient_state);

			if ((state.current_time % 60) == 0) {
				printf("%d/%d/%d\n", state.col_tests, state.col2_tests, state.col_mask_tests);
			}
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

			tex.upload_subimage(img.slice());

#if !BONK_USE_GL2
			glDisable(GL_BLEND);
#endif

			win.textured.use();
			buf.color(1, 1, 1, 1);
			buf.quads();

			buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, canvasw / 512.f, canvash / 512.f, tex.id);

			buf.clear();
		};

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

		do {
			if (!win.update()) { return; }

			for (auto& ev : win.events) {
				TL_UNUSED(ev);
				//
			}

#if GFX_PREDICT_VSYNC
			win.draw_delay += draw_delay;
#endif
			win.events.clear();

		} while (!win.end_drawing());
	}
}
