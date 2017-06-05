#include "gui.hpp"
#include "../gfx/geom_buffer.hpp"

namespace gui {

void Context::render(gfx::GeomBuffer& geom) {
	u8* buf_begin = this->buf.begin();

	usize pos = 0;
	usize s = this->buf.size();

	tl::VectorI2 placement(10, 10);

	geom.color(0, 0, 0, 1);
	geom.set_texture(0);

	while (pos < s) {
		WindowDesc const* desc = (WindowDesc const *)(buf_begin + pos);

		pos += desc->size;
		switch (desc->type) {
			case 0:
				ButtonDesc const* button_desc = (ButtonDesc const*)desc;
				geom.quads();
				geom.vertex(tl::VectorF2(placement.x, placement.y));
				geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y));
				geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y + button_desc->win_size.h));
				geom.vertex(tl::VectorF2(placement.x, placement.y + button_desc->win_size.h));
				geom.vertex_color(4);

				placement.y += 3 + button_desc->win_size.h + 3;
				break;
		}
	}

	geom.clear();
}

static void render_window(Context& context, gfx::GeomBuffer& geom, tl::VectorI2& placement, usize pos) {
	u8* buf_begin = context.buf.begin();

	WindowDesc const* desc = (WindowDesc const *)(buf_begin + pos);

	switch (desc->type) {
	case 0: {
		ButtonDesc const* button_desc = (ButtonDesc const*)desc;

		geom.color(0, 0, 0, 1);
		geom.set_texture(0);
		geom.quads();
		geom.vertex(tl::VectorF2(placement.x, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y + button_desc->win_size.h));
		geom.vertex(tl::VectorF2(placement.x, placement.y + button_desc->win_size.h));

		geom.flush();

		if (desc->attrib & WindowDesc::Focus) {
			geom.color(1, 1, 1, 1);
		} else {
			geom.color(0.6, 0.6, 0.6, 1);
		}
		context.font.draw_text(geom,
			tl::StringSlice(button_desc->text, button_desc->text + button_desc->text_size),
			tl::VectorF2(placement.x + 10.f, placement.y + 5.f), 4.f);

		geom.flush();

		placement.y += 3 + button_desc->win_size.h + 3;
		break;
	}

	case 1: {
		FrameDesc const* frame_desc = (FrameDesc const *)desc;

		usize const *cur_child = frame_desc->children, *end_child = (usize const *)((u8 const *)frame_desc + frame_desc->size);

		for (; cur_child != end_child; ++cur_child) {
			render_window(context, geom, placement, *cur_child);
		}
		break;
	}

	}
}

void Context::render2(gfx::GeomBuffer& geom) {
	u8* buf_begin = this->buf.begin();

	assert(this->children.size() == 1);
	usize pos = this->children[0];

	tl::VectorI2 placement(10, 10);

	geom.color(0, 0, 0, 1);
	geom.set_texture(0);

	render_window(*this, geom, placement, pos);

	this->children.clear();
	this->buf.clear();

	geom.clear();
}

static void render_window(Context3& context, gfx::GeomBuffer& geom, tl::VectorI2& placement, Context3::Window* window) {

	switch (window->type) {
	case 0:
	{
		Context3::Button* button_desc = (Context3::Button *)window;

		geom.color(0, 0, 0, 1);
		geom.set_texture(0);
		geom.quads();
		geom.vertex(tl::VectorF2(placement.x, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y + button_desc->win_size.h));
		geom.vertex(tl::VectorF2(placement.x, placement.y + button_desc->win_size.h));

		geom.flush();

		if (button_desc->attrib & WindowDesc::Focus) {
			geom.color(1, 1, 1, 1);
		} else {
			geom.color(0.6, 0.6, 0.6, 1);
		}

		context.font.draw_text(geom,
			button_desc->text.slice_const(),
			tl::VectorF2(placement.x + 10.f, placement.y + 5.f), 4.f);

		geom.flush();

		placement.y += 3 + button_desc->win_size.h + 3;
		break;
	}

	case 1:
	{
		Context3::Frame* frame_desc = (Context3::Frame *)window;

		for (auto& c : frame_desc->children()) {
			render_window(context, geom, placement, &c);
		}
		break;
	}

	}
}

void Context3::render2(gfx::GeomBuffer& geom) {
	tl::VectorI2 placement(10, 10);

	geom.color(0, 0, 0, 1);
	geom.set_texture(0);

	for (auto& c : this->cur_parent->children()) {
		render_window(*this, geom, placement, &c);
	}

	geom.clear();
}

}
