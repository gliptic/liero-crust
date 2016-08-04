#include <tl/cstdint.h>

namespace gfx {

i16 const gfx_keys_to_native[kbRangeEnd + 1] = {
	-1,
	native_kbEscape,
	native_kbF1,
	native_kbF2,
	native_kbF3,
	native_kbF4,
	native_kbF5,
	native_kbF6,
	native_kbF7,
	native_kbF8,
	native_kbF9,
	native_kbF10,
	native_kbF11,
	native_kbF12,
	native_kb0,
	native_kb1,
	native_kb2,
	native_kb3,
	native_kb4,
	native_kb5,
	native_kb6,
	native_kb7,
	native_kb8,
	native_kb9,
	native_kbTab,
	native_kbReturn,
	native_kbSpace,
	native_kbLeftShift,
	native_kbRightShift,
	native_kbLeftControl,
	native_kbRightControl,
	native_kbLeftAlt,
	native_kbRightAlt,
	native_kbLeftMeta,
	native_kbRightMeta,
	native_kbBackspace,
	native_kbLeft,
	native_kbRight,
	native_kbUp,
	native_kbDown,
	native_kbHome,
	native_kbEnd,
	native_kbInsert,
	native_kbDelete,
	native_kbPageUp,
	native_kbPageDown,
	native_kbEnter,
	native_kbNumpad0,
	native_kbNumpad1,
	native_kbNumpad2,
	native_kbNumpad3,
	native_kbNumpad4,
	native_kbNumpad5,
	native_kbNumpad6,
	native_kbNumpad7,
	native_kbNumpad8,
	native_kbNumpad9,
	native_kbNumpadAdd,
	native_kbNumpadSubtract,
	native_kbNumpadMultiply,
	native_kbNumpadDivide,
	native_kbMinus,
	native_kbEquals,
	
	native_kbQ,
	native_kbW,
	native_kbE,
	native_kbR,
	native_kbT,
	native_kbY,
	native_kbU,
	native_kbI,
	native_kbO,
	native_kbP,
	native_kbLeftBracket,
	native_kbRightBracket,
	native_kbA,
	native_kbS,
	native_kbD,
	native_kbF,
	native_kbG,
	native_kbH,
	native_kbJ,
	native_kbK,
	native_kbL,
	native_kbSemicolon,
	native_kbApostrophe,
	native_kbGrave,    /* accent grave */
	native_kbBackslash,
	native_kbZ,
	native_kbX,
	native_kbC,
	native_kbV,
	native_kbB,
	native_kbN,
	native_kbM,
};

i16 gfx_native_to_keys[native_kbRangeEnd + 1];

static void init_keymaps() {
	i16 i;
	for (i = 0; i <= native_kbRangeEnd; ++i) {
		gfx_native_to_keys[i] = kbUnknown;
	}
	
	for (i = kbRangeBegin; i <= kbRangeEnd; ++i) {
		if (gfx_keys_to_native[i] >= 0)
			gfx_native_to_keys[gfx_keys_to_native[i]] = i;
	}
}

}
