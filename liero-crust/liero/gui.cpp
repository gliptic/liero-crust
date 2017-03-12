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
		geom.quads();
		geom.vertex(tl::VectorF2(placement.x, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y));
		geom.vertex(tl::VectorF2(placement.x + button_desc->win_size.w, placement.y + button_desc->win_size.h));
		geom.vertex(tl::VectorF2(placement.x, placement.y + button_desc->win_size.h));
		geom.vertex_color(4);

		placement.y += 3 + button_desc->win_size.h + 3;
		break;
	}

	case 1: {
		FrameDesc const* frame_desc = (FrameDesc const *)desc;
		//usize children_count = desc->size - desc->children

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

	geom.clear();
}

}
