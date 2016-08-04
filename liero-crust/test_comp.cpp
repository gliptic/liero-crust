#include "gfx/window.hpp"
#include "gfx/texture.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include <tl/stream.hpp>

#include "bmp.hpp"

typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

struct DomainInfo {
	tl::VectorU2 range;
	u8 avg;
};

u8 clamp_byte(i32 v) {
	return v < 0 ? 0 : (v >= 256 ? 255 : (u8)v);
}

void diff(tl::ImageSlice src, tl::ImageSlice other, tl::ImageSlice diff) {
	
	auto srcr = src.lines_range();
	auto otherr = other.lines_range();
	auto diffr = diff.lines_range();

	tl::ImageSlice::PixelsRange<u8> srcp, otherp, diffp;

	u32 y = 0;

	u8 pred[256];
	for (u32 i = 0; i < 256; ++i) {
		pred[i] = i;
	}

	while (srcr.next(srcp)) {
		otherr.next(otherp);
		diffr.next(diffp);

		u8 prevsrc = 0;

		u8 *srcptr = srcp.cur, *otherptr = otherp.cur, *diffptr = diffp.cur;

		for (u32 x = 0; x < srcp.size(); ++x) {
			
			prevsrc = *srcptr;
		}

		++y;
	}
}

template<typename Process>
void fractal(tl::ImageSlice src, tl::ImageSlice target, Process process) {
	u32 cell_size = 4;
	u32 range_size = cell_size * 2;
	u32 cdim_y = src.height() / cell_size - 1;
	u32 cdim_x = src.width() / cell_size - 1;

	u32 extra = 0;

	tl::Vec<DomainInfo> info;

	for (auto range_pos : tl::RectU(tl::VectorU2(0, 0), cdim_x, cdim_y).row_major_range()) {
		auto range_rect = tl::RectU(range_pos * cell_size, cell_size + extra);
		auto domain_s = src.crop(range_rect);

		u32 domain_sum = 0;
		auto r = domain_s.range();
		for (u8* p; (p = r.next()) != 0; ) {
			domain_sum += *p;
		}

		u8 domain_avg = u8(domain_sum / ((cell_size + extra) * (cell_size + extra)));

		u32 lowest_mse = 0xffffffff;
		i32 lowest_adjust_to_domain;
		tl::VectorU2 lowest_range;

		i32 dist = 64;

		//u32 low_ry = min(max(i32(cpy) - dist, 0), i32(cdim_y) - 1 - dist * 2);
		//u32 low_rx = min(max(i32(cpx) - dist, 0), i32(cdim_x) - 1 - dist * 2);
		u32 low_ry = 0, low_rx = 0;
		u32 high_ry = min(low_ry + dist * 2, i32(cdim_y) - 1);
		u32 high_rx = min(low_rx + dist * 2, i32(cdim_x) - 1);

		/*
		
		*/
		
		for (u32 ry = low_ry; ry < high_ry; ++ry)
		for (u32 rx = low_rx; rx < high_rx; ++rx) {
			u32 rpx = rx * cell_size;
			u32 rpy = ry * cell_size;

			auto range_s = src.crop(tl::RectU(rpx, rpy, rpx + range_size + extra * 2, rpy + range_size + extra * 2));

			u32 range_sum = 0;
			auto r = range_s.range();
			for (u8* p; (p = r.next()) != 0; ) {
				range_sum += *p;
			}

			u8 range_avg = u8(range_sum / ((range_size + extra * 2) * (range_size + extra * 2)));

			i32 adjust_to_domain = range_avg - domain_avg;
			u32 mse = 0;

			for (u32 py = 0; py < cell_size + extra; ++py)
			for (u32 px = 0; px < cell_size + extra; ++px) {
				u8 dp = domain_s.unsafe_pixel8(px, py);
				u8 rp = u8((u32(range_s.unsafe_pixel8(px * 2, py * 2)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2)) + 
						u32(range_s.unsafe_pixel8(px * 2, py * 2 + 1)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2 + 1))) / 4);

				rp = clamp_byte((i32)rp + adjust_to_domain);
				i32 err = i32(rp) - i32(dp);

				err *= err;

				if (extra) {
					u32 total = extra * extra;
					u32 weight = (cell_size + extra - px) * (cell_size + extra - py);
					//u32 total = extra + extra;
					//u32 weight = (cell_size + extra - px) + (cell_size + extra - py);

					if (weight > total) weight = total;

					err = u64(err) * weight / total;
				}

				mse += err;
			}

			if (mse < lowest_mse) {
				lowest_range = tl::VectorU2(rx, ry);
				lowest_mse = mse;
				lowest_adjust_to_domain = adjust_to_domain;
			}
		}

		if (true) {
			u32 rx = lowest_range.x;
			u32 ry = lowest_range.y;
			u32 rpx = rx * cell_size;
			u32 rpy = ry * cell_size;

			i32 adjust_to_domain = lowest_adjust_to_domain;

			auto range_s = src.crop(tl::RectU(rpx, rpy, rpx + range_size, rpy + range_size));

			for (u32 py = 0; py < cell_size; ++py)
			for (u32 px = 0; px < cell_size; ++px) {
				u8 rp = u8((u32(range_s.unsafe_pixel8(px * 2, py * 2)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2)) + 
						u32(range_s.unsafe_pixel8(px * 2, py * 2 + 1)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2 + 1))) / 4);

				rp = clamp_byte((i32)rp + adjust_to_domain);

				domain_s.unsafe_pixel8(px, py) = rp;
			}
		}

		DomainInfo i;
		i.avg = domain_avg;
		i.range = lowest_range;
		info.push_back(i);

		if (range_pos.x == 0) {
			printf("%d%%\n", (range_pos.y * cdim_y + range_pos.x) * 100 / (cdim_x * cdim_y));
			process();
		}
	}

	for (u32 iter = 0; iter < 8; ++iter) {
		
		for (u32 cy = cdim_y; cy-- > 0; )
		for (u32 cx = cdim_x; cx-- > 0; ) {
			DomainInfo* di = &info[cy * cdim_x + cx];

			u32 cpx = cx * cell_size;
			u32 cpy = cy * cell_size;
			
			tl::VectorU2 range = di->range;
			u8 domain_avg = di->avg;
			++di;

			auto domain_s = target.crop(tl::RectU(cpx, cpy, cpx + cell_size + extra, cpy + cell_size + extra));

			u32 rpx = range.x * cell_size;
			u32 rpy = range.y * cell_size;
			auto range_s = src.crop(tl::RectU(rpx, rpy, rpx + range_size + extra * 2, rpy + range_size + extra * 2));

			u32 range_sum = 0;
			auto r = range_s.range();
			for (u8* p; (p = r.next()) != 0; ) {
				range_sum += *p;
			}

			u8 range_avg = u8(range_sum / ((range_size + extra * 2) * (range_size + extra * 2)));

			i32 adjust_to_domain = i32(range_avg) - i32(domain_avg);

			for (u32 py = 0; py < cell_size + extra; ++py)
			for (u32 px = 0; px < cell_size + extra; ++px) {
				u8 rp = u8((u32(range_s.unsafe_pixel8(px * 2, py * 2)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2)) + 
						u32(range_s.unsafe_pixel8(px * 2, py * 2 + 1)) +
						u32(range_s.unsafe_pixel8(px * 2 + 1, py * 2 + 1))) / 4);

				rp = clamp_byte((i32)rp + adjust_to_domain);

				u8& dp = domain_s.unsafe_pixel8(px, py);

				if (extra) {
					u32 total = extra * extra;
					u32 weight = (cell_size + extra - px) * (cell_size + extra - py);
					//u32 total = extra + extra;
					//u32 weight = (cell_size + extra - px) + (cell_size + extra - py);

					if (weight > total) weight = total;

					dp = (dp * (total - weight) + rp * weight) / total;
				} else {
					dp = rp;
				}
			}
		}
	}
}

void test_comp() {
	int scrw = 1024, scrh = 768;

	gfx::CommonWindow win;
	win.set_mode(scrw, scrh, 0);
	win.set_visible(1);

	int canvasw = scrw, canvash = scrh;
	int texsize = 2048;
	gfx::Texture tex(2048, 2048);

	tl::Image bmp_img;

	tl::Image canvas(canvasw, canvash, 4);
	
	memset(canvas.data(), 0, canvas.size());

	
	PFNWGLSWAPINTERVALEXTPROC sw = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	sw(1);

	{
		tl::Palette pal;
		//auto src = tl::source::from_file("C:\\Users\\glip\\Documents\\cpp\\marfpu\\_build\\TC\\test.bmp");
		auto src = tl::source::from_file("C:\\Users\\glip\\Documents\\cpp\\marfpu\\_build\\TC\\lena512.bmp");
		int r = read_bmp(src, bmp_img, pal);
	}

	tl::Image dest_img(bmp_img.width(), bmp_img.height(), bmp_img.bytespp());

	gfx::GeomBuffer buf;

	fractal(bmp_img.slice(), dest_img.slice(), [&]{
		if (!win.end_drawing()) {
			if (!win.update()) { return; }

			for (auto& ev : win.events) {
				TL_UNUSED(ev);
				//
			}

			//win.draw_delay += draw_delay;
			win.events.clear();
		} else {
			win.clear();

			{
				canvas.blit(bmp_img.slice(), 0, 0, 0, tl::ImageSlice::AsGrayscale);
				canvas.blit(dest_img.slice(), 0, 512, 0, tl::ImageSlice::AsGrayscale);
			}

			tex.upload_subimage(canvas.slice());
		
			glDisable(GL_BLEND);
			buf.color(1, 1, 1, 1);
			buf.quads();

			buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, (float)canvasw / texsize, (float)canvash / texsize, tex.id);

			TL_TIME(draw_delay, {
				buf.clear();
			});
			glEnable(GL_BLEND);
		}
	});
	
	while (true) {
		win.clear();

		{
			canvas.blit(bmp_img.slice(), 0, 0, 0, tl::ImageSlice::AsGrayscale);
			canvas.blit(dest_img.slice(), 0, 512, 0, tl::ImageSlice::AsGrayscale);
		}

		tex.upload_subimage(canvas.slice());
		
		glDisable(GL_BLEND);
		buf.color(1, 1, 1, 1);
		buf.quads();

		buf.tex_rect(0.f, 0.f, (float)scrw, (float)scrh, 0.f, 0.f, (float)canvasw / texsize, (float)canvash / texsize, tex.id);

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