#include <tl/cstdint.h>

namespace liero {

/* The liero materials in use are:
0
dirt | back
dirt2 | back
dirt
dirt2
rock
worm
back | seeshadow
back

We can get rid of dirt2 because it's only used for selecting better colors.
We don't have to support shadows and we can ignore worm material in levels.
This gives us:

0
dirt | back
dirt
rock
back

This only uses 3 bits, so we have 5 bits to do something with!

*/ 

struct Material {

	enum {
		Dirt = 1<<0,
		Rock = 1<<1,
		Background = 1<<2,
		WormM = 1<<3
	};

	u8 flags;

	explicit Material(u8 flags)
		: flags(flags) {
	}

	bool rock() const { return (flags & Rock) != 0; }
	bool background() const { return (flags & Background) != 0; }
	
	// Constructed
	bool dirt_rock() const { return (flags & (Dirt | Rock)) != 0; }
	bool any_dirt() const { return (flags & Dirt) != 0; }
	bool dirt_back() const { return (flags & (Dirt | Background)) != 0; }
	bool worm() const { return (flags & WormM) != 0; }
};

}
