#include <tl/std.h>
#include <tl/string.hpp>
#include <tl/vec.hpp>
#include <tl/memory.h>
#include <tl/region.hpp>
#include "../gfx/geom_buffer.hpp"
#include "../gfx/window.hpp"
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
	enum struct Type : u32 {
		Frame = 0,
		Button = 1
	};

	struct Window : tl::TreeNode<Window> {
		enum {
			Focus = 1 << 0
		};

		u64 id;
		//isize size;
		u32 attrib;
		Type type;

		tl::Rect render_pos;
	};

	struct Container : Window {
	};

	struct Frame : Container {
		static Type const OwnType = Type::Frame;
	};

	struct Button : Window {
		static Type const OwnType = Type::Button;

		Size win_size;
		tl::String text;
	};

	typedef tl::TreeNode<Window>::ChildRange ChildRange;

	Container* cur_parent;
	tl::TreeNode<Window>::ChildRange cur_child;
	u64 focus_id;
	u32 focus_mask;
	Window* focus_win; // Set after generating UI
	//bool event_down, event_up, event_left, event_right; // TEMP. TODO: More elaborate event system.
	//bool event_activate;

	u32 dep_version;
	gfx::Event event;
	Font& font;

	Context3(Font& font)
		: focus_mask(0)
		, dep_version(0)
		, event(gfx::Event::none())
		, font(font) {

		this->cur_parent = new Frame();
		this->focus_id = 1;
	}

	struct Scope {
		Context3& context;
		Container* old_parent;
		ChildRange old_cur_child;

		Scope(Context3& context, Container* new_parent)
			: context(context) {

			this->old_parent = context.cur_parent;
			this->old_cur_child = context.cur_child;
			context.cur_parent = new_parent;
			context.cur_child = new_parent->children();
		}

		~Scope() {
			this->context.cur_child.cut_rest();
			
			this->context.cur_parent = this->old_parent;
			this->context.cur_child = this->old_cur_child;
		}
	};

	template<typename F>
	void frame(u64 id, F f) {
		Frame* frame = 0;
		auto cur = this->cur_child; // cur_parent->children();
		
		while (cur.has_more()) {
			auto* c = cur.next();
			if (c->id == id && c->type == Type::Frame) {
				// TODO: Diff attributes
				frame = (Frame *)c;
				break;
			}
		}

		if (!frame) {
			frame = new Frame();
			frame->type = Type::Frame;
			frame->id = id;
		} else {
			frame->unlink();
		}

		frame->attrib = 0;

		this->cur_child.link_sibling_after(frame);

		Scope scope(*this, frame);
		f();
	}


	template<typename T>
	T* find_window(tl::TreeNode<Window>::ChildRange children, u64 id) {
		T* w = 0;
		while (children.has_more()) {
			auto* c = children.next();
			if (c->id == id && c->type == T::OwnType) {
				w = (T *)c;
				break;
			}
		}

		return w;
	}

	void control(Window* win, u64 id) {
		if (win->id == this->focus_id) {
			this->focus_win = win;
		}
	}

	/*
	bool button(u64 id, Size size, tl::StringSlice text) {
		return this->button(id, size, text, [] {});
	}*/

	enum struct ButtonEvent {
		None,
		Focus,
		Press
	};

	ButtonEvent button(u64 id, Size size, tl::StringSlice text) {

		Button* button = find_window<Button>(this->cur_child, id);

		// TODO: Better focus handling. Store away
		// currently focused window in context.

		u32 new_attrib = id == this->focus_id ? WindowDesc::Focus : 0;

		if (!button) {
			button = new Button();
			button->type = Type::Button;
			button->text = tl::String(text);
			button->id = id;
		} else {
			if (button->text.slice_const() != text) {
				button->text = tl::String(text);
			}
			button->unlink();
		}

		button->win_size = size;
		button->attrib = new_attrib;

		control(button, id);

		ButtonEvent retEvent = ButtonEvent::None;

		bool focused = id == this->focus_id;

		if (focused) {
			if (this->event.ev == gfx::ev_button && this->event.d.button.id == kbEnter) {
				retEvent = ButtonEvent::Press;
			} else {
				retEvent = ButtonEvent::Focus;
			}
		}

		this->cur_child.link_sibling_after(button);
		return retEvent;
	}

	template<typename F>
	void traverse(F f) {
		bool found = false;
		tl::Vec<tl::TreeNode<Window>::ChildRange> parents;
		auto cur_range = this->cur_parent->children();

		for (;;) {
		begin:
			if (!cur_range.has_more()) {
				if (parents.size() > 0) {
					cur_range = parents.back();
					parents.pop_back();
					goto begin;
				} else {
					break;
				}
			}

			auto* c = cur_range.next();

			f(c);

			if (c->has_children()) {
				parents.push_back(cur_range);
				cur_range = c->children();
			}
		}
	}

	void render2(gfx::GeomBuffer& geom);

	bool is_masked(int button) {
		if (button < kbLeft || button > kbDown) {
			return false;
		}

		return (focus_mask & (1 << (button - kbLeft))) != 0;
	}

	bool is_navigation(int button) {
		printf("butt: %d\n", button);
		return (!is_masked(kbDown) && button == kbDown)
			|| (!is_masked(kbUp) && button == kbUp)
			|| (!is_masked(kbLeft) && button == kbLeft)
			|| (!is_masked(kbRight) && button == kbRight);
	}

	bool start(u32 dep_version) {
		this->cur_child = this->cur_parent->children();

		bool changes = this->event.ev != gfx::ev_none;

		if (this->event.ev == gfx::ev_button && is_navigation(this->event.d.button.id) && this->focus_win) {

			auto focus_point = this->focus_win->render_pos.center();

			Context3::Window* closest = 0;
			i32 closest_dist = 0x7fffffff;

			this->traverse([&](Context3::Window* w) {
				if (w->type == Context3::Type::Button) {
					auto dir = w->render_pos.center() - focus_point;
					if ((this->event.d.button.id == kbDown  && dir.y > 0 && std::abs(dir.x) < std::abs(dir.y))
					 || (this->event.d.button.id == kbUp    && dir.y < 0 && std::abs(dir.x) < std::abs(dir.y))
					 || (this->event.d.button.id == kbRight && dir.x > 0 && std::abs(dir.y) < std::abs(dir.x))
					 || (this->event.d.button.id == kbLeft  && dir.x < 0 && std::abs(dir.y) < std::abs(dir.x))) {

						// Direction down
						auto dist = dir.x * dir.x + dir.y * dir.y;
						if (dist < closest_dist) {
							closest = w;
							closest_dist = dist;
						}
					}
				}
			});

			this->event = gfx::Event::none();

			if (closest) {
				this->focus_id = closest->id;
				this->focus_mask = 0;
			}
		}

		changes = changes || dep_version != this->dep_version;
		this->dep_version = dep_version;

		if (changes) {
			this->focus_win = 0;
		}

		return changes;
	}
};

/*
struct WeaponSelect {
	int i;
};
*/

}

#include "../lierosim/mod.hpp"

namespace gui {

struct LieroGui {

	enum Mode {
		Main,
		WeaponSelect,
		Game
	};

	LieroGui(liero::PlayerSettingsBuilder playerSettings)
		: mode(Game), players(playerSettings) { // TEMP: mode(Main)
		
		for (int i = 0; i < 2; ++i) {
			for (int w = 0; w < 5; ++w) {
				//players[i]->weapons()[w] = w;
			}
		}
	}

	Mode mode;
	liero::PlayerSettingsBuilder players;

	template<typename Context>
	void run(Context& ctx, u32& dep_version, liero::Mod& mod) {
		if (ctx.start(dep_version)) {
			auto regular_size = Size(200, 30);

			Context3::Scope scope(ctx, ctx.cur_parent);

			if (this->mode == Main) {

				ctx.frame(0, [&, this] {
					if (ctx.button(1, regular_size, "New game"_S) == Context3::ButtonEvent::Press) {
						this->mode = WeaponSelect;
						ctx.focus_id = 6;
						++dep_version;
					}

					ctx.button(2, regular_size, "Settings"_S);
					ctx.button(3, regular_size, "Quit"_S);
				});
			} else if (this->mode == WeaponSelect) {
				ctx.frame(5, [&, this] {
					int playerId = 0;
					for (int i = 0; i < 5; ++i) {
#if 1
						u16& wid = this->players.weapons()[i];

						if (ctx.button(6 + i, regular_size, mod.weapon_types[wid].name()) == Context3::ButtonEvent::Focus) {
							ctx.focus_mask = (1 << 0) | (1 << 1);
							if (ctx.event.ev == gfx::ev_button) {
								if (ctx.event.d.button.id == kbLeft) {
									wid = (wid + 40 - 1) % 40;
									++dep_version;
								} else if (ctx.event.d.button.id == kbRight) {
									wid = (wid + 1) % 40;
									++dep_version;
								}

								ctx.event.ev = gfx::ev_none;
							}
						}
#endif
					}
				});
			}

		}
	}
};

}
