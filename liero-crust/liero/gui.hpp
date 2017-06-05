#include <tl/std.h>
#include <tl/string.hpp>
#include <tl/vec.hpp>
#include <tl/memory.h>
#include <tl/region.hpp>
#include "../gfx/geom_buffer.hpp"
#include "font.hpp"
#include <tl/tree.hpp>

namespace gui {

struct Size {
	Size() : w(0), h(0) {}

	Size(u32 w, u32 h) : w(w), h(h) {
	}

	u32 w, h;
};

struct WindowDesc {
	enum {
		Focus = 1 << 0
	};

	isize size;
	u32 attrib;
	u32 type;
};

struct FrameDesc : WindowDesc {
	usize children[];
};

struct ButtonDesc : WindowDesc {
	Size win_size;
	u32 text_size;
	u8 text[0];
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
	using Control<Context>::Control;

	template<typename Contents>
	void with(Contents contents) {
		contents();
	}

	u32 w, h;
};

struct Id {
	u64 a, b;

	Id() : a(0), b(0) {
	}

	Id(tl::StringSlice v) {
		if (v.size() <= 15) {
			
		}
	}
};

struct Context {
	tl::BufferMixed buf;
	tl::Vec<usize> children;
	Font& font;
	u64 focus_id;

	Context(Font& font) : font(font), focus_id(0) {

	}

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

	template<typename F>
	Frame<Context> frame(u64 id, F f) {
		usize size_old = children.size();
		f();
		usize size_new = children.size();
		usize children_size = (size_new - size_old) * sizeof(usize);
		auto frame_desc = this->alloc<FrameDesc>(children_size);
		frame_desc->attrib = 0;
		frame_desc->type = 1;
		memcpy(frame_desc->children, children.begin(), children_size);
		children.unsafe_set_size(size_old);

		this->children.push_back(frame_desc.offset);
		return Frame<Context>(*this, frame_desc);
	}

	Button<Context> button(u64 id, Size size, tl::StringSlice text) {
		auto text_size = text.size_bytes();
		auto button_desc = this->alloc<ButtonDesc>(text_size);
		button_desc->attrib = id == focus_id ? WindowDesc::Focus : 0;
		button_desc->type = 0;
		button_desc->win_size = size;
		button_desc->text_size = text_size;
		memcpy(button_desc->text, text.begin(), text_size);

		this->children.push_back(button_desc.offset);
		return Button<Context>(*this, button_desc);
	}

	void render(gfx::GeomBuffer& geom);
	void render2(gfx::GeomBuffer& geom);
};

struct Context3 {
	struct Window : tl::TreeNode<Window> {
		enum {
			Focus = 1 << 0
		};

		u64 id;
		isize size;
		u32 attrib;
		u32 type;
	};

	struct Container : Window {
	};

	struct Frame : Container {
	};

	struct Button : Window {
		Size win_size;
		tl::String text;
	};

	Container* cur_parent;
	u64 focus_id;
	Font& font;

	Context3(Font& font) : font(font) {
		cur_parent = new Frame();
		focus_id = 1;
	}

	template<typename F>
	void frame(u64 id, F f) {
		Frame* frame = 0;
		auto cur = cur_parent->children();
		
		while (cur.has_more()) {
			auto* c = cur.next();
			if (c->id == id && c->type == 1) {
				// TODO: Diff attributes
				frame = (Frame *)c;
				break;
			}
		}

		if (!frame) {
			frame = new Frame();
			frame->attrib = 0;
			frame->type = 1;
			frame->id = id;
			//cur_parent->children.push_back(frame);
			cur.link_sibling_after(frame);
		}

		auto* old_parent = cur_parent;
		cur_parent = frame;
		f();
		cur_parent = old_parent;
	}

	void button(u64 id, Size size, tl::StringSlice text) {
		Button* button = 0;
		auto cur = cur_parent->children();

		while (cur.has_more()) {
			auto* c = cur.next();
			if (c->id == id && c->type == 0) {
				// TODO: Diff attributes
				button = (Button *)c;
				break;
			}
		}

		if (!button) {
			button = new Button();
			button->attrib = id == focus_id ? WindowDesc::Focus : 0;
			button->type = 0;
			button->win_size = size;
			button->text = tl::String(text);
			button->id = id;
			//cur_parent->children.push_back(button);
			cur.link_sibling_after(button);
		}
	}

	void render2(gfx::GeomBuffer& geom);
};

struct LieroGui {

	template<typename Context>
	void run(Context& ctx) {
		ctx.frame(0, [&] {
			ctx.button(1, Size(200, 30), "Foo"_S);
			ctx.button(2, Size(200, 30), "Bar"_S);
		});
	}
};

}
