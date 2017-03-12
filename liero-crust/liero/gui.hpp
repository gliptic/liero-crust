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

struct FrameDesc : WindowDesc {
	usize children[];
};

struct ButtonDesc : WindowDesc {
	Size win_size;
};

template<typename T>
struct Ref {
	T* p;
	usize offset;

	T* operator->() {
		return p;
	}
};

template<typename Context>
struct Control {
	template<typename T>
	Control(Context& ctx_init, Ref<T> ref)
		: ctx(ctx_init)
		, desc_offset(ref.offset) {
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
	/*
	Frame(Context& ctx_init)
		: Control<Context>(ctx_init) {
	}*/
	using Control<Context>::Control;

	template<typename Contents>
	void with(Contents contents) {
		contents();
	}

	u32 w, h;
};

struct Context {
	tl::BufferMixed buf;
	tl::Vec<usize> children;

	template<typename T>
	Ref<T> alloc(usize extra_size = 0) {
		usize size = sizeof(T) + extra_size;
		u8* desc = buf.unsafe_alloc(size);
		usize offs = usize(desc - buf.begin());

		Ref<T> ref;
		ref.offset = offs;
		ref.p = (T *)desc;
		ref.p->size = size;
		return ref;
	}

	/*
	Button<Context> button() {
		return Button<Context>(*this);
	}

	Frame<Context> frame() {
		return Frame<Context>(*this);
	}
	*/

	template<typename F>
	Frame<Context> frame(F f) {
		usize size_old = children.size();
		f();
		usize size_new = children.size();
		usize children_size = (size_new - size_old) * sizeof(usize);
		auto frame_desc = this->alloc<FrameDesc>(children_size);
		frame_desc->type = 1;
		memcpy(frame_desc->children, children.begin(), children_size);
		children.unsafe_set_size(size_old);

		this->children.push_back(frame_desc.offset);
		return Frame<Context>(*this, frame_desc);
	}

	Button<Context> button2(Size size = Size()) {
		auto button_desc = this->alloc<ButtonDesc>();
		button_desc->type = 0;
		button_desc->win_size = size;

		this->children.push_back(button_desc.offset);
		return Button<Context>(*this, button_desc);
	}

	void render(gfx::GeomBuffer& geom);
	void render2(gfx::GeomBuffer& geom);
};

struct LieroGui {

	template<typename Context>
	void run(Context& ctx) {
		ctx.frame([&] {
			ctx.button2(Size(10, 10));
			ctx.button2(Size(10, 10));
		});
	}
};

}
