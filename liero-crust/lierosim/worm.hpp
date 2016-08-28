#ifndef LIERO_WORM_HPP
#define LIERO_WORM_HPP 1

#include <liero-sim/config.hpp>
#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>

namespace liero {

struct State;
struct StateInput;
struct Mod;
struct ModRef;
struct TransientState;
struct Worm;

struct ControlState	{
	ControlState()
	: istate(0)	{
	}

	bool operator==(ControlState const& b) const {
		return istate == b.istate;
	}

	u32 pack() const {
		return istate;
	}

	void unpack(u32 state)	{
		istate = state & 0x7f;
	}

	bool operator!=(ControlState const& b) const {
		return !operator==(b);
	}

	bool operator[](u32 n) const {
		return ((istate >> n) & 1) != 0;
	}

	void set(u32 n, bool v) {
		istate = (istate & ~(u32(1u) << n)) | (u32(v) << n);
	}

	void toggle(u32 n) {
		istate ^= 1 << n;
	}

	u32 istate;
};

struct WormTransientState {
	enum FlagsGfx : u8 {
		Animate = (1 << 0)
	};

	WormTransientState()
		: gfx_flags(0) {
	}

	ControlState controls;
	u8 gfx_flags;

	bool animate() const {
		return (gfx_flags & Animate) != 0;
	}
};

struct WormWeapon {
	u32 ty_idx;
	u32 ammo, delay_left, loading_left;

	WormWeapon()
		: ty_idx(0), ammo(0), delay_left(0), loading_left(0) {
	}
};

static u8 const worm_anim_tab[] = {
	0,
	7,
	0,
	14
};

struct Ninjarope {
	enum St : u8 {
		Hidden,
		Floating,
		Attached
	};

	Ninjarope() : st(Hidden) {
	}

	void update(Worm& owner, State& state);

	Vector2 pos, vel;
	f64 length;
	St st;
};

struct Worm {
	enum WormControl {
		Up = 0, Down, Left, Right,
		Fire, Change, Jump,
		Dig,
		MaxControl = Dig,
		MaxControlEx
	};

	static u32 const SelectableWeapons = 40;

	Worm() :
		aiming_angle(0), aiming_angle_vel(0), current_weapon(0), leave_shell_time(0), direction(1),
		muzzle_fire(0) {
	}

	Vector2 pos, vel;
	Ninjarope ninjarope;
	Scalar aiming_angle, aiming_angle_vel;

	WormWeapon weapons[SelectableWeapons]; // TODO: Adjustable?
	u32 current_weapon;
	u32 leave_shell_time;

	ControlState control_flags;

	u32 muzzle_fire; // GFX

	// TODO: Direction is one bit. We can store it as a flag
	i8 direction; // 1 = right, -1 = left

	Scalar abs_aiming_angle() const {
		return direction > 0 ? aiming_angle : Fixed(64) - aiming_angle;
	}

	u32 current_frame(u32 current_time, WormTransientState const& transient_state) const {

		i32 x = -(i32)this->aiming_angle + 32 - 12;

		x >>= 3;
		if (x < 0) x = 0;
		else if (x > 6) x = 6;

		u32 angle_frame = (u32)x;
		u32 anim_frame = transient_state.animate() ? (current_time & 31) >> 3 : 0;
		return angle_frame + worm_anim_tab[anim_frame];
	}

	bool movable() const {
		return true;
	}

	void update(State& state, TransientState& state_input, u32 index);
	void reset_weapons(ModRef& mod);
	void spawn(State& state);
};

}

#endif // LIERO_WORM_HPP
