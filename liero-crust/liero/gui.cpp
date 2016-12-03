#include "gui.hpp"
#include "../gfx/geom_buffer.hpp"

namespace gui {

void Context::render(gfx::GeomBuffer& geom) {
	u8* buf = this->buf.begin();

	usize pos = 0;
	usize s = this->buf.size();

	tl::VectorI2 placement(10, 10);

	geom.color(0, 0, 0, 1);
	geom.set_texture(0);

	while (pos < s) {
		WindowDesc const* desc = (WindowDesc const *)(buf + pos);

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
				break;
		}
	}

	geom.clear();
}

}
