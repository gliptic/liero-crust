#include <tl/std.h>
#include <tl/string.hpp>

struct Control {

	TL_DEFAULT_CTORS(Control);
};

struct Button : Control {
	TL_DEFAULT_CTORS(Button);

	Button& text(tl::StringSlice text) {
		return *this;
	}

	Button& click() {
		return *this;
	}

	~Button() {
	}
};

struct Frame : Control {
	template<typename Contents>
	void with(Contents contents) {
		contents();
	}
};

struct Context {
	Button button() {
		return Button();
	}

	Frame frame() {
		return Frame();
	}
};

struct LieroGui : Context {

	void run() {
		frame().with([=] {
			this->button()
				.text("Hello"_S)
				.click();
		});
	}
};