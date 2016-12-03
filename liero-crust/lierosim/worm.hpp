#ifndef LIERO_WORM_HPP
#define LIERO_WORM_HPP 1

#include <liero-sim/config.hpp>
#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>
#include <tl/rand.hpp>

namespace liero {

struct State;
struct StateInput;
struct Mod;
struct ModRef;
struct TransientState;
struct Worm;
struct Level;

enum WormControl {
	WcLeft = 0, WcRight,
	WcChange, WcJump,
	WcFire, WcUp, WcDown,
	WcDig,
	WcMaxControl = WcDig,
	WcMaxControlEx
};

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

struct WormInput {
	u8 v;

	enum struct Action : u8 {
		Jump = 1 << 4, Fire = 2 << 4,

		Change = 4 << 4,
		Ninjarope = 5 << 4,

		Mask = 7 << 4
	};

	enum struct Move : u8 {
		None = 0 << 2,
		Left = 1 << 2,
		Right = 2 << 2,
		Dig = 3 << 2,

		Mask = 3 << 2
	};

	enum struct Aim : u8 {
		None = 0,
		Up = 1,
		Down = 2,
		Both = 3,

		Mask = 3
	};

	/*
	enum struct ChangeDir : u8 {
		None = 0 << 2,
		Left = 1 << 2,
		Right = 2 << 2,

		Mask = 3 << 2
	};
	*/

	WormInput(u8 v_init = 0) : v(v_init) {
		assert(v < 6 * 4 * 4);
	}

	WormInput(u8 act, u8 move, u8 aim) : v(act | move | aim) {
	}

	static WormInput from_keys(ControlState state) {
		u8 aim = (state[WcUp] ? (u8)Aim::Up : 0)
			| (state[WcDown] ? (u8)Aim::Down : 0);

		if (state[WcChange]) {
			Move dir = Move::None;
			if (state[WcLeft] && !state[WcRight]) {
				dir = Move::Left;
			} else if (!state[WcLeft] && state[WcRight]) {
				dir = Move::Right;
			}

			if (state[WcJump]) {
				return ninjarope(dir, (Aim)aim);
			} else {
				return change(dir, (Aim)aim);
			}
		} else {
			Move dir = Move::None;
			if (state[WcLeft] && state[WcRight]) {
				dir = Move::Dig;
			} else if (state[WcLeft] && !state[WcRight]) {
				dir = Move::Left;
			} else if (!state[WcLeft] && state[WcRight]) {
				dir = Move::Right;
			}

			return combo(state[WcJump], state[WcFire], dir, (Aim)aim);
		}
	}

	static WormInput change(Move dir, Aim aim) {
		return WormInput((u8)Action::Change | (u8)dir | (u8)aim);
	}

	static WormInput ninjarope(Move dir, Aim aim) {
		return WormInput((u8)Action::Ninjarope | (u8)dir | (u8)aim);
	}

	static WormInput combo(bool jump, bool fire, Move move, Aim aim) {
		return WormInput(
			(jump ? (u8)Action::Jump : 0) |
			(fire ? (u8)Action::Fire : 0) |
			(u8)move | (u8)aim);
	}

	bool rope() const { return (v & (u8)Action::Mask) == (u8)Action::Ninjarope; }
	bool change() const { return (v & (u8)Action::Mask) == (u8)Action::Change; }
	bool change_or_rope() const { return v >= (u8)Action::Change; }

	bool jump() const { return (v & ((u8)Action::Mask & ~(u8)Action::Fire)) == (u8)Action::Jump; }
	bool fire() const { return (v & ((u8)Action::Mask & ~(u8)Action::Jump)) == (u8)Action::Fire; }

	bool left() const { return (v & (u8)Move::Left) != 0; }
	bool right() const { return (v & (u8)Move::Right) != 0; }
	bool dig() const { return (v & ((u8)Move::Left | (u8)Move::Right)) == ((u8)Move::Left | (u8)Move::Right); }

	bool up() const { return (v & (u8)Aim::Up) != 0; }
	bool down() const { return (v & (u8)Aim::Down) != 0; }
};

struct WormTransientState {
	enum FlagsGfx : u8 {
		Animate = (1 << 0)
	};

	WormTransientState()
		: gfx_flags(0)
		, steerable_count(0) {
	}

	WormInput input;
	u8 gfx_flags;
	u32 steerable_count;

	bool animate() const {
		return (gfx_flags & Animate) != 0;
	}

	// TODO: After left or right is held, and steerable_count is non-zero, this should not return until
	// both left/right are released and steerable_count is 0.
	bool movable() const {
		return steerable_count == 0;
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
	static u32 const SelectableWeapons = 40;

	Worm() :
		aiming_angle(0), aiming_angle_vel(0), current_weapon(0), leave_shell_time(0),
		health(0),
		muzzle_fire(0), bits0(PrevNoLeft | PrevNoRight | PrevNoJump) {
	}

	Vector2 pos, vel;
	Ninjarope ninjarope;
	Scalar aiming_angle, aiming_angle_vel;

	WormWeapon weapons[SelectableWeapons]; // TODO: Adjustable?
	u32 current_weapon;
	i32 health;
	u32 leave_shell_time;

	u32 muzzle_fire; // GFX

	u8 bits0;

	enum {
		PrevNoLeft = 0,
		PrevNoRight = 1,
		PrevChange = 2,
		PrevNoJump = 3
	};

	void prev_controls(u32 controls) {
		bits0 = (bits0 & ~15) | (controls & 15);
	}

	void prev_controls(WormInput input) {
		bits0 = (bits0 & ~15)
			| ((u8)input.change_or_rope() << PrevChange)
			| ((u8)!(input.jump() || input.rope()) << PrevNoJump)
			| ((u8)!input.left() << PrevNoLeft)
			| ((u8)!input.right() << PrevNoRight);
	}

	void direction(u8 dir) {
		assert(dir == 0 || dir == 1);
		bits0 = (bits0 & ~16) | (dir << 4);
	}

	u8 direction() const {
		return (bits0 >> 4) & 1;
	}

	bool prev_no_left() const { return (bits0 & (1 << PrevNoLeft)) != 0; }
	bool prev_no_right() const { return (bits0 & (1 << PrevNoRight)) != 0; }
	bool prev_change() const { return (bits0 & (1 << PrevChange)) != 0; }
	bool prev_no_jump() const { return (bits0 & (1 << PrevNoJump)) != 0; }

	bool prev_change_or_no_dig() const { return prev_change() || prev_no_left() || prev_no_right(); }
	bool prev_no_jump_nor_change() const { return prev_no_jump() && !prev_change(); }
	bool prev_no_jump_or_no_change() const { return prev_no_jump() || !prev_change(); }

	Scalar abs_aiming_angle() const {
		return direction() == 0 ? aiming_angle : Fixed(64) - aiming_angle;
	}

	u32 angle_frame() const {
		i32 x = -(i32)this->aiming_angle + 32 - 12;

		x >>= 3;
		if (x < 0) x = 0;
		else if (x > 6) x = 6;

		return (u32)x;
	}

	u32 current_frame(u32 current_time, WormTransientState const& transient_state) const {

		u32 anim_frame = transient_state.animate() ? (current_time & 31) >> 3 : 0;
		return angle_frame() + worm_anim_tab[anim_frame];
	}

	void do_damage(i32 amount) {
		health -= amount;
	}

	void update(State& state, TransientState& state_input, u32 index);
	void reset_weapons(ModRef& mod);
	void spawn(State& state);
};

tl::VectorI2 find_spawn(Level& level, tl::LcgPair& rand);

}

#endif // LIERO_WORM_HPP
