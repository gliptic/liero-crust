#include <tl/std.h>
#include <tl/string.hpp>
#include <tl/vec.hpp>
#include "../gfx/geom_buffer.hpp"

namespace gui {

struct Size {
	Size() : w(0), h(0) {}

	Size(u32 w, u32 h) : w(w), h(h) {
	}

	u32 w, h;
};

struct WindowDesc {
	isize size;
	u32 type;
};

struct ButtonDesc : WindowDesc {
	Size win_size;
};

template<typename Context>
struct Control {
	Control(Context& ctx_init) : ctx(ctx_init) {
	}

	Context& ctx;
	usize desc_offset;
};

template<typename Context>
struct Button : Control<Context> {
	using Control<Context>::Control;

	~Button() {
	}
};

template<typename Context>
struct Frame : Control<Context> {
	Frame(Context& ctx_init)
		: Control<Context>(ctx_init) {
	}

	template<typename Contents>
	void with(Contents contents) {
		contents();
	}

	u32 w, h;
};

struct Context {
	tl::BufferMixed buf;

	Button<Context> button() {
		return Button<Context>(*this);
	}

	Frame<Context> frame() {
		return Frame<Context>(*this);
	}

	Button<Context> button2(Size size = Size()) {
		u8* desc = buf.unsafe_alloc(sizeof(ButtonDesc));
		isize offs = desc - buf.begin();

		ButtonDesc* button_desc = (ButtonDesc *)desc;
		button_desc->size = sizeof(ButtonDesc);
		button_desc->type = 0;
		button_desc->win_size = size;

		return Button<Context>(*this);
	}

	void render(gfx::GeomBuffer& geom);
};

struct LieroGui {

	template<typename Context>
	void run(Context& ctx) {
		ctx.button2(Size(10, 10));
	}
};

}
