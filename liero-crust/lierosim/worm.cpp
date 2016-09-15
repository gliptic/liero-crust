#include "worm.hpp"
#include "state.hpp"
#include <tl/approxmath/am.hpp>

namespace liero {

namespace {

enum Direction : u8 {
	DirDown = 0,
	DirLeft,
	DirUp,
	DirRight
};

struct PixelCount {
	PixelCount() {
		counts[0] = 0;
		counts[1] = 0;
		counts[2] = 0;
		counts[3] = 0;
	}

	u8& operator[](usize idx) {
		return counts[idx];
	}

	u8 counts[4];
};

static u8 count_pixels(State& state, tl::VectorI2 pos, Direction dir) {
	u8 count = 0;

	if (dir == DirDown || dir == DirUp) {
		i32 y = dir == DirDown ? pos.y - 4 : pos.y + 4;

		for (i32 x = pos.x - 1; x <= pos.x + 1; ++x) {
			if (!state.level.mat_wrap_back(tl::VectorI2(x, y))) {
				++count;
			}
		}
	} else {
		i32 x = dir == DirRight ? pos.x - 1 : pos.x + 1;

		for (i32 y = pos.y - 3; y <= pos.y + 3; ++y) {
			if (!state.level.mat_wrap_back(tl::VectorI2(x, y))) {
				++count;
			}
		}
	}

	return count;
}

inline PixelCount count_worm_pixels(Worm& worm, State& state) {

	auto next = worm.pos + worm.vel;
	auto inext = next.cast<i32>();

	PixelCount pixel_counts;

	pixel_counts[DirDown] += count_pixels(state, inext, DirDown);
	pixel_counts[DirLeft] += count_pixels(state, inext, DirLeft);
	pixel_counts[DirUp] += count_pixels(state, inext, DirUp);
	pixel_counts[DirRight] += count_pixels(state, inext, DirRight);

	if (inext.x < 4)
		pixel_counts[DirRight] += 20;
	else if (inext.x > (i32)state.level.graphics.width() - 5)
		pixel_counts[DirLeft] += 20;

	if (inext.y < 5)
		pixel_counts[DirDown] += 20;
	else if (inext.y > (i32)state.level.graphics.height() - 6)
		pixel_counts[DirUp] += 20;

	// Up/down pushing

	if (pixel_counts[DirDown] < 2
	 && pixel_counts[DirUp]
	 && (pixel_counts[DirLeft] || pixel_counts[DirRight])) {

		worm.pos.y -= Scalar(1);
		next.y = worm.pos.y + worm.vel.y;
		inext.y = i32(next.y);

		pixel_counts[DirLeft] = count_pixels(state, inext, DirLeft);
		pixel_counts[DirRight] = count_pixels(state, inext, DirRight);
	}

	if (pixel_counts[DirUp] < 2
	 && pixel_counts[DirDown]
	 && (pixel_counts[DirLeft] || pixel_counts[DirRight])) {

		worm.pos.y += Scalar(1);
		next.y = worm.pos.y + worm.vel.y;
		inext.y = i32(next.y);

		pixel_counts[DirLeft] = count_pixels(state, inext, DirLeft);
		pixel_counts[DirRight] = count_pixels(state, inext, DirRight);
	}

	return pixel_counts;
}

inline void update_aiming(Worm& worm, State& state, WormTransientState& transient_state) {

	ModRef& mod = state.mod;
	auto& input = transient_state.input;
	bool up = input.up();
	bool down = input.down();

	worm.aiming_angle += worm.aiming_angle_vel;

	if (!up && !down) {
		worm.aiming_angle_vel *= mod.tcdata->aim_fric_mult();
	}

	// TODO: Correct constants

	if (worm.aiming_angle > Scalar(20)) {
		worm.aiming_angle_vel = Scalar(0);
		worm.aiming_angle = Scalar(20);
	} else if (worm.aiming_angle < Scalar(-32)) {
		worm.aiming_angle_vel = Scalar(0);
		worm.aiming_angle = Scalar(-32);
	}

	if (transient_state.movable()
	 && (worm.ninjarope.st == Ninjarope::Hidden || !input.change_or_rope())) {

		if (up && worm.aiming_angle_vel > -mod.tcdata->max_aim_vel()) {
			worm.aiming_angle_vel -= mod.tcdata->aim_acc();
		}

		if (down && worm.aiming_angle_vel < mod.tcdata->max_aim_vel()) {
			worm.aiming_angle_vel += mod.tcdata->aim_acc();
		}
	}
}

inline void update_actions(Worm& worm, u32 worm_idx, State& state, WormInput input, PixelCount pixel_counts, TransientState& transient_state) {

	ModRef& mod = state.mod;

	if (input.change_or_rope()) {
		if (worm.ninjarope.st != Ninjarope::Hidden) {
			if (input.up()) worm.ninjarope.length -= mod.tcdata->nr_pull_vel();
			if (input.down()) worm.ninjarope.length += mod.tcdata->nr_release_vel();

			if (worm.ninjarope.length < mod.tcdata->nr_min_length())
				worm.ninjarope.length = mod.tcdata->nr_min_length();
			if (worm.ninjarope.length > mod.tcdata->nr_max_length())
				worm.ninjarope.length = mod.tcdata->nr_max_length();
		}

		// !Jump | !Change -> Jump & Change
		if (worm.prev_no_jump_or_no_change() && input.rope()) {
			worm.ninjarope.st = Ninjarope::Floating;

			transient_state.play_sound(mod, mod.tcdata->throw_sound(), transient_state); // TODO: 5 should be stored in TC

			worm.ninjarope.pos = worm.pos;
			worm.ninjarope.vel = sincos(worm.abs_aiming_angle()) * mod.tcdata->nr_throw_vel();
			worm.ninjarope.length = mod.tcdata->nr_initial_length();
		}

		worm.muzzle_fire = 0;

		// TODO: if(weapons[currentWeapon].available() || game.settings->loadChange)    ...maybe
		if (true) {
			if (worm.prev_no_left() && input.left()) {
				if (worm.current_weapon-- == 0) {
					worm.current_weapon = Worm::SelectableWeapons - 1;
				}

				// TODO: hotspotX/Y? That should be in rendering code
			}

			if (worm.prev_no_right() && input.right()) {
				if (++worm.current_weapon >= Worm::SelectableWeapons) {
					worm.current_weapon = 0;
				}

				// TODO: hotspotX/Y? That should be in rendering code
			}
		}
	} else {

		// !Left | !Right | Change -> Left & Right & !Change
		if (input.dig() && worm.prev_change_or_no_dig()) {

			auto step = sincos(worm.abs_aiming_angle()) * 2;
			auto dig_pos = worm.pos + step;

			// TODO: TC should have dig effect constant
			draw_level_effect(state, dig_pos.cast<i32>(), 7, transient_state.graphics);
			draw_level_effect(state, (dig_pos + step).cast<i32>(), 7, transient_state.graphics);
		}

		// !Jump & !Change -> Jump & !Change
		if (worm.prev_no_jump_nor_change() && input.jump()) {

			worm.ninjarope.st = Ninjarope::Hidden;

			if (pixel_counts[DirUp]) {
				worm.vel.y -= mod.tcdata->jump_force();
			}
		}

		WormWeapon& ww = worm.weapons[worm.current_weapon];

		if (input.fire() && ww.loading_left == 0 && ww.delay_left == 0) {
			WeaponType const& w = mod.get_weapon_type(ww.ty_idx);
			--ww.ammo;
			ww.delay_left = w.delay();
			// TODO: worm.muzzle_fire = w.muzzle_fire;

			fire(w, state, transient_state, worm.abs_aiming_angle(), worm.vel, worm.pos - Vector2(0, 1), tl::narrow<i16>(worm_idx));

			worm.vel += sincos(worm.abs_aiming_angle()) * w.recoil();
		}

		// TODO: if(weapons[currentWeapon].type->loopSound) game.soundPlayer->stop(&weapons[currentWeapon]);
	}
}

inline void update_movements(Worm& worm, State& state, WormTransientState& transient_state) {

	auto input = transient_state.input;

	if (!input.change_or_rope() && transient_state.movable()) {
		ModRef& mod = state.mod;
		bool left = input.left();
		bool right = input.right();

		if (left ^ right) {
			u8 new_dir = left;
			i8 sign = new_dir ? -1 : 1;

			if (worm.vel.x * sign < mod.tcdata->max_vel())
				worm.vel.x += mod.tcdata->walk_vel() * sign;

			if (worm.direction() != new_dir) {
				worm.aiming_angle_vel = Scalar(0);
			}

			worm.direction(new_dir);
			transient_state.gfx_flags |= WormTransientState::Animate;
		}
	}
}

inline void update_physics(Worm& worm, State& state, PixelCount pixel_counts) {
	ModRef& mod = state.mod;

	if (pixel_counts[DirUp]) {
		worm.vel.x *= mod.tcdata->worm_fric_mult();
	}

	Vector2 absvel(fabs(worm.vel.x), fabs(worm.vel.y));
	
	i32 rh = pixel_counts[worm.vel.x >= Scalar(0) ? DirLeft : DirRight];
	i32 rv = pixel_counts[worm.vel.y >= Scalar(0) ? DirUp : DirDown];
	Scalar mb = mod.tcdata->min_bounce();

	f64 const bounce_factor = -1.0 / 3.0; // TDDO: Store

	if (rh) {
		if(absvel.x > mb) { // TODO: We wouldn't need the vel.x check if we knew that mbh/mbv were always at least liero_eps
#if 0 // TODO
			if(common.H[HFallDamage])
				health -= LC(FallDamageRight);
			else
				game.soundPlayer->play(14);
#endif
			worm.vel.x *= bounce_factor;
		} else {
			worm.vel.x = Scalar(0);
		}
	}

	if (rv) {
		if (absvel.y > mb) {
#if 0 // TODO
			if(common.H[HFallDamage])
				health -= LC(FallDamageDown);
			else
				game.soundPlayer->play(14);
#endif
			worm.vel.y *= bounce_factor;
		} else {
			worm.vel.y = Scalar(0);
		}
	}
	
	if (pixel_counts[DirUp] == 0) {
		worm.vel.y += mod.tcdata->worm_gravity();
	}

	// No, we can't use rh/rv here, they are out of date. TODO: Move this earlier so that we can?
	if (pixel_counts[worm.vel.x >= Scalar(0) ? DirLeft : DirRight] < 2)
		worm.pos.x += worm.vel.x;
	
	if (pixel_counts[worm.vel.y >= Scalar(0) ? DirUp : DirDown] < 2)
		worm.pos.y += worm.vel.y;
}

inline void update_weapons(Worm& worm, State& state) {

	for (u32 i = 0; i < Worm::SelectableWeapons; ++i) {
		if (worm.weapons[i].delay_left) {
			--worm.weapons[i].delay_left;
		}
	}

	WormWeapon& ww = worm.weapons[worm.current_weapon];
	WeaponType const& ty = state.mod.get_weapon_type(ww.ty_idx);

	if (ww.ammo == 0) {
		u32 loading_time = computed_loading_time(ty);
		ww.loading_left = loading_time;
		ww.ammo = ty.ammo();
	}

	if (ww.loading_left) {
		--ww.loading_left;
		if (ww.loading_left == 0 && ty.play_reload_sound()) {
			// game.soundPlayer->play(24); TODO
		}
	}

	if (worm.muzzle_fire) { // TODO: Express this as a time as well
		--worm.muzzle_fire;
	}

	if (worm.leave_shell_time == state.current_time) {
		auto shell_vel = rand_max_vector2(state.rand, tl::VectorD2(10000.0 / 2147483648.0, 8000.0 / 2147483648.0));
		shell_vel.y -= Scalar::from_raw(10000);

		create(state.mod.get_nobject_type(40 + 7), state, Scalar(), worm.pos, shell_vel);
	}
}

inline void update_steerables(Worm& worm, u32 worm_index, State& state, WormTransientState& transient_state) {
	auto& w = worm.weapons[worm.current_weapon];
	auto& weapon_ty = state.mod.get_weapon_type(w.ty_idx);
	auto& nobj_ty = state.mod.get_nobject_type(weapon_ty.nobject_type());

	u32 count = 0;
	tl::VectorI2 sum;

	if (nobj_ty.kind() == NObjectKind::Steerable) {
		auto r = state.nobjects.all();
		for (NObject* nobj; (nobj = r.next()) != 0; ) {
			if (nobj->ty_idx == w.ty_idx && nobj->owner == tl::narrow<i16>(worm_index)) {
				if (transient_state.input.left()) {
					nobj->cur_frame -= 1 + (state.current_time & 1);
				}
				if (transient_state.input.right()) {
					nobj->cur_frame += 1 + (state.current_time & 1);
				}

				nobj->cur_frame &= 127;
				++count;
				sum += nobj->pos.cast<i32>();
			}
		}
	}

	transient_state.steerable_count = count;
}

}

void Worm::reset_weapons(ModRef& mod) {

	for (u32 i = 0; i < Worm::SelectableWeapons; ++i) {
		auto& ww = this->weapons[i];
		auto const& ty = mod.get_weapon_type(ww.ty_idx);
		ww.ammo = ty.ammo();
		ww.delay_left = 0;
		ww.loading_left = 0;
	}
}

void Worm::spawn(State& state) {
	this->reset_weapons(state.mod);
}

void Worm::update(State& state, TransientState& transient_state, u32 index) {
	PixelCount pixel_counts;

	auto& worm_transient_state = transient_state.worm_state[index];

	if (true) { // TODO: is_visible

		pixel_counts = count_worm_pixels(*this, state);

		// TODO: Bonus coldet?

		update_steerables(*this, index, state, worm_transient_state);
		update_aiming(*this, state, worm_transient_state);
		update_weapons(*this, state);
		update_actions(*this, index, state, worm_transient_state.input, pixel_counts, transient_state);
		update_physics(*this, state, pixel_counts);
		update_movements(*this, state, worm_transient_state);

		/* TODO: This is a graphics thing only
		processSight(game);
		*/
	}

	this->prev_controls(worm_transient_state.input);
}

void Ninjarope::update(Worm& owner, State& state) {
	ModRef& mod = state.mod;
	
	if (this->st != Ninjarope::Hidden) {
		this->pos += this->vel;
		
		auto ipos = this->pos.cast<i32>();
		
#if 0 // TODO
		anchor = 0;
		for(std::size_t i = 0; i < game.worms.size(); ++i)
		{
			Worm& w = *game.worms[i];
			
			if(&w != &owner
			&& checkForSpecWormHit(game, ipos.x, ipos.y, 1, w))
			{
				anchor = &w;
				break;
			}
		}
#endif
		
		auto diff = this->pos - owner.pos;

		Vector2 force(diff * mod.tcdata->nr_force_mult());
		
		// TODO: f64(16) really depends on nr_force_len_shl
		auto cur_len = tl::length(diff.cast<f64>()) + f64(16);

		Material m(Material::Rock); // Treat borders as rock

		if (state.level.mat_dirt_rock(ipos)) {

			if (this->st != Ninjarope::Attached) {

				this->st = Ninjarope::Attached;
				this->length = mod.tcdata->nr_attach_length();

				if (state.level.mat_dirt(ipos)) {
					// TODO: Throw up some dirt. Try to use bobjects
#if 0
					PalIdx pix = game.level.pixel(ipos);
					for(int i = 0; i < 11; ++i) // TODO: Check 11 and read from exe
					{
						common.nobjectTypes[2].create2(
							game,
							game.rand(128),
							fixedvec(),
							pos,
							pix,
							owner.index,
							0);
					}
#endif
				}

				this->vel.zero();
			}
#if 0 // TODO
		} else if (anchor) {
			if(!attached)
			{
				length = LC(NRAttachLength); // TODO: Should this value be separate from the non-worm attaching?
				attached = true;
			}
			
			if(curLen > length)
			{
				anchor->vel -= force / curLen;
			}
			
			vel = anchor->vel;
			pos = anchor->pos;
#endif
		} else {
			this->st = Ninjarope::Floating;
		}
		
		if (this->st == Ninjarope::Attached) {
			// cur_len can't be 0

			if (cur_len > this->length) {
				owner.vel += force / cur_len;
			}

		} else {
			this->vel.y += mod.tcdata->ninjarope_gravity();

			if (cur_len > this->length) {
				this->vel -= force / cur_len;
			}
		}
	}
}

}

