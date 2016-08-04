#include "worm.hpp"
#include "state.hpp"
#include <tl/approxmath/am.hpp>

namespace liero {

namespace {

enum Direction {
	DirDown,
	DirLeft,
	DirUp,
	DirRight
};


static u32 count_pixels(State& state, tl::VectorI2 pos, Direction dir) {
	u32 count = 0;

	if (dir == DirDown || dir == DirUp) {
		i32 y = dir == DirDown ? pos.y - 4 : pos.y + 4;

		for (i32 x = pos.x - 1; x <= pos.x + 1; ++x) {
			if (!state.level.mat_wrap(tl::VectorI2(x, y)).background()) {
				++count;
			}
		}
	} else {
		i32 x = dir == DirRight ? pos.x - 1 : pos.x + 1;

		for (i32 y = pos.y - 3; y <= pos.y + 3; ++y) {
			if (!state.level.mat_wrap(tl::VectorI2(x, y)).background()) {
				++count;
			}
		}
	}

	return count;
}


inline void count_worm_pixels(Worm& worm, State& state, u32 pixel_counts[4]) {

	Mod& mod = state.mod;
	auto next = worm.pos + worm.vel;
	auto inext = next.cast<i32>(); 

	pixel_counts[DirDown] = count_pixels(state, inext, DirDown);
	pixel_counts[DirLeft] = count_pixels(state, inext, DirLeft);
	pixel_counts[DirUp] = count_pixels(state, inext, DirUp);
	pixel_counts[DirRight] = count_pixels(state, inext, DirRight);

	if (inext.x < 4)
		pixel_counts[DirRight] += 20;
	else if (inext.x > (i32)state.level.materials.width() - 5)
		pixel_counts[DirLeft] += 20;

	if (inext.y < 5)
		pixel_counts[DirDown] += 20;
	else if (inext.y > (i32)state.level.materials.height() - 6)
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
}

inline void update_aiming(Worm& worm, State& state, ControlState controls) {

	Mod& mod = state.mod;
	bool up = controls[Worm::Up];
	bool down = controls[Worm::Down];

	worm.aiming_angle += worm.aiming_angle_vel;

	if (!up && !down) {
		worm.aiming_angle_vel *= LF(AimFricMult); // TODO
	}

	// TODO: Correct constants
#if LIERO_FIXED
	if (worm.aiming_angle > Scalar(20)) {
		worm.aiming_angle_vel = Scalar(0);
		worm.aiming_angle = Scalar(20);
	} else if (worm.aiming_angle < Scalar(-32)) {
		worm.aiming_angle_vel = Scalar(0);
		worm.aiming_angle = Scalar(-32);
	}
#else
	if (worm.aiming_angle > tl::pi * 0.3) {
		worm.aiming_angle_vel = 0.0;
		worm.aiming_angle = tl::pi * 0.3;
	} else if (worm.aiming_angle < tl::pi * -0.5) {
		worm.aiming_angle_vel = 0.0;
		worm.aiming_angle = tl::pi * -0.5;
	}
#endif

	if (worm.movable()) { // TODO: if(movable && (!ninjarope.out || !pressed(Change)))
		if (up && worm.aiming_angle_vel > -LF(MaxAimVel)) {
			worm.aiming_angle_vel -= LF(AimAcc);
		}

		if (down && worm.aiming_angle_vel < LF(MaxAimVel)) {
			worm.aiming_angle_vel += LF(AimAcc);
		}
	}
}

inline void update_actions(Worm& worm, State& state, ControlState controls, u32 (&pixel_counts)[4]) {

	Mod& mod = state.mod;

	if (controls[Worm::Change]) {
#if 0 // TODO
		if(ninjarope.out)
		{
			if(pressed(Up))
				ninjarope.length -= LF(NRPullVel); 
			if(pressed(Down))
				ninjarope.length += LF(NRReleaseVel);
				
			if(ninjarope.length < LF(NRMinLength))
				ninjarope.length = LF(NRMinLength);
			if(ninjarope.length > LF(NRMaxLength))
				ninjarope.length = LF(NRMaxLength);
		}
		
		if(pressedOnce(Jump))
		{
			ninjarope.out = true;
			ninjarope.attached = false;
			
			game.soundPlayer->play(5);
			
			ninjarope.pos = pos;
			ninjarope.vel = fixedvec(cossinTable[ftoi(aimingAngle)].x << LF(NRThrowVelX), cossinTable[ftoi(aimingAngle)].y << LF(NRThrowVelY));
									
			ninjarope.length = LF(NRInitialLength);
		}
#endif
		// TODO: 

		worm.muzzle_fire = 0;

		// TODO: if(weapons[currentWeapon].available() || game.settings->loadChange)    ...maybe
		if (true) {
			if (!worm.control_flags[Worm::Left] && controls[Worm::Left]) {
				if (worm.current_weapon-- == 0) {
					worm.current_weapon = Worm::SelectableWeapons - 1;
				}

				// TODO: hotspotX/Y? That should be in rendering code
			}

			if (!worm.control_flags[Worm::Right] && controls[Worm::Right]) {
				if (++worm.current_weapon >= Worm::SelectableWeapons) {
					worm.current_weapon = 0;
				}

				// TODO: hotspotX/Y? That should be in rendering code
			}
		}
	} else {

		if (controls[Worm::Jump]) {
			// TODO: ninjarope.out = false;
			// TODO: ninjarope.attached = false;
			
			if((pixel_counts[DirUp] /* TODO: || common.H[HAirJump] */)
			&& (!worm.control_flags[Worm::Jump] /* || common.H[HMultiJump]*/))
			{
				worm.vel.y -= LF(JumpForce);
				worm.control_flags.set(Worm::Jump, true);
			} else {
				worm.control_flags.set(Worm::Jump, false);
			}
		}

		WormWeapon& ww = worm.weapons[worm.current_weapon];

		if (controls[Worm::Fire] && ww.loading_left == 0 && ww.delay_left == 0) {
			WeaponType const& w = mod.get_weapon_type(ww.ty_idx);
			--ww.ammo;
			ww.delay_left = w.delay;
			// TODO: worm.muzzle_fire = w.muzzle_fire;

			w.fire(state, worm.abs_aiming_angle(), worm.vel, worm.pos - Vector2(0, 1));

			/* TODOs
			int recoil = w.recoil;
	
			if(common.H[HSignedRecoil] && recoil >= 128)
				recoil -= 256;
	
			vel -= cossinTable[ftoi(aimingAngle)] * recoil / 100;
			*/
		}

		// TODO: if(weapons[currentWeapon].type->loopSound) game.soundPlayer->stop(&weapons[currentWeapon]);
	}
}

inline void update_movements(Worm& worm, State& state, ControlState controls) {

	if (!controls[Worm::Change] && worm.movable()) {
		Mod& mod = state.mod;
		bool left = controls[Worm::Left];
		bool right = controls[Worm::Right];

		worm.flags_gfx &= ~Worm::Animate;

		if (left && right) {
			// TODO: Dig
		} else if (left || right) {
			i8 new_dir = left ? -1 : 1;

			if (worm.vel.x * new_dir < LF(MaxVel))
				worm.vel.x += LF(WalkVel) * new_dir;

			if (worm.direction != new_dir) {
				worm.aiming_angle_vel = Scalar(0);
			}

			worm.direction = new_dir;
			worm.flags_gfx |= Worm::Animate;
		}
	}
}

inline void update_physics(Worm& worm, State& state, u32 (&pixel_counts)[4]) {
	Mod& mod = state.mod;

	if (pixel_counts[DirUp])
		worm.vel.x *= LF(WormFricMult);

	Vector2 absvel(fabs(worm.vel.x), fabs(worm.vel.y));
	
	i32 rh = pixel_counts[worm.vel.x >= Scalar(0) ? DirLeft : DirRight];
	i32 rv = pixel_counts[worm.vel.y >= Scalar(0) ? DirUp : DirDown];
	Scalar mbh = worm.vel.x > Scalar(0) ? LF(MinBounceRight) : -LF(MinBounceLeft); // TODO: Do we really need different constants for left/right?
	Scalar mbv = worm.vel.y > Scalar(0) ? LF(MinBounceDown) : -LF(MinBounceUp);

	Scalar const bounce_factor = Scalar(-1.0 / 3.0);

	if (rh) {
		if(absvel.x >= liero_eps && absvel.x > mbh) { // TODO: We wouldn't need the vel.x check if we knew that mbh/mbv were always at least liero_eps
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
		if (absvel.y >= liero_eps && absvel.y > mbv) {
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
		worm.vel.y += LF(WormGravity);
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
		u32 computed_loading_time = ty.computed_loading_time();
		ww.loading_left = computed_loading_time;
		ww.ammo = ty.ammo;
	}

	if (ww.loading_left) {
		--ww.loading_left;
		if (ww.loading_left == 0 && ty.play_reload_sound) {
			// game.soundPlayer->play(24); TODO
		}
	}

	if (worm.muzzle_fire) {
		--worm.muzzle_fire;
	}

	/* TODO:
	if(leaveShellTimer > 0)
	{
		if(--leaveShellTimer <= 0)
		{
			auto velY = -int(game.rand(20000));
			auto velX = game.rand(16000) - 8000;
			common.nobjectTypes[7].create1(game, fixedvec(velX, velY), pos, 0, index, 0);
		}
	}
	*/
}

}

void Worm::reset_weapons(Mod& mod) {

	for (u32 i = 0; i < Worm::SelectableWeapons; ++i) {
		auto& ww = this->weapons[i];
		auto const& ty = mod.get_weapon_type(ww.ty_idx);
		ww.ammo = ty.ammo;
		ww.delay_left = 0;
		ww.loading_left = 0;
	}
}

void Worm::spawn(State& state) {
	this->reset_weapons(state.mod);
}

void Worm::update(State& state, StateInput const& state_input) {
	u32 pixel_counts[4];

	auto controls = state_input.worm_controls[this->index];

	if (true) { // TODO: is_visible

		count_worm_pixels(*this, state, pixel_counts);

		// TODO: Bonus coldet?

		// TODO: processSteerables?
		//bool movable = true; // TODO: Set to false when steering

		update_aiming(*this, state, controls);
		update_weapons(*this, state);
		update_actions(*this, state, controls, pixel_counts);
		update_physics(*this, state, pixel_counts);
		update_movements(*this, state, controls);

		/* TODO: This is a graphics thing only
		processSight(game);
		*/
	}

	this->control_flags = controls;
}

}

