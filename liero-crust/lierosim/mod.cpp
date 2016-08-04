#include "mod.hpp"
#include "state.hpp"
#include "../tga.hpp"

#include <tl/approxmath/am.hpp>

namespace liero {

static f64 const radians_to_liero = 128.0 / 6.283185307179586476925286766559;


tl::Image read_full_tga(tl::source file, tl::Palette* pal_out = 0) {
	tl::Image tga;
	tl::Palette pal;
	int c;
	
	{
		c = read_tga(file, tga, pal);
		tl::Image tgaFull(tga.width(), tga.height(), 4);
		c = tgaFull.blit(tga.slice(), &pal);

		if (pal_out) {
			*pal_out = pal;
		}

		return std::move(tgaFull);
	}
}

tl::Image read_full_tga(char const* path, tl::Palette* pal_out = 0) {
	return read_full_tga(tl::source::from_file(path), pal_out);	
}

Mod::Mod() {
	tl::FsNode root(new tl::FsNodeFilesystem("C:\\Users\\glip\\Documents\\cpp\\marfpu\\_build\\TC\\lierov133winxp"_S));

	auto sprites = root / "sprites"_S;
	this->large_sprites = read_full_tga((sprites / "large.tga"_S).try_get_source(), &this->pal);
	this->small_sprites = read_full_tga((sprites / "small.tga"_S).try_get_source(), &this->pal);
	this->small_font_sprites = read_full_tga((sprites / "text.tga"_S).try_get_source(), &this->pal);

	worm_sprites[0].alloc_uninit(16, 7 * 3 * 16, 4);
	worm_sprites[1].alloc_uninit(16, 7 * 3 * 16, 4);

	auto worms = large_sprites.crop(tl::RectU(0, 16 * 16, 16, 16 * (16 + 7 * 3)));
	worm_sprites[0].blit(worms);
	worm_sprites[1].blit(worms, 0, 0, 0, tl::ImageSlice::Flip);

	sobject_types[0] = SObjectType::hellraider_smoke();
	sobject_types[1] = SObjectType::large_explosion();
	sobject_types[2] = SObjectType::small_explosion();
	
	nobject_types[0] = NObjectType::small_damage(*this, 2);
	nobject_types[1] = NObjectType::bazooka(0, 0, 1);
	nobject_types[2] = NObjectType::medium_damage(*this, 2);
	nobject_types[3] = NObjectType::float_mine(2, 1);
	nobject_types[4] = NObjectType::fan(*this);

	weapon_types[0] = WeaponType::bazooka(1);
	weapon_types[1] = WeaponType::float_mine(3);
	weapon_types[2] = WeaponType::fan(4);

	for (auto& n : nobject_types) {
		n.idx = (u32)(&n - &nobject_types[0]);
	}

	for (auto& s : sobject_types) {
		s.idx = (u32)(&s - &sobject_types[0]);
	}

	for (auto& w : weapon_types) {
		w.idx = (u32)(&w - &weapon_types[0]);
	}

	Mod& mod = *this;

	LC(MaxVel) = raw(29184_lf);
	LC(WalkVel) = raw(3000_lf);
	LC(MaxAimVel) = raw(70000_langrel);
	LC(AimAcc) = raw(4000_langrel);
	LC(WormFricMult) = raw(Scalar(0.89));
	LC(AimFricMult) = raw(Scalar(0.83));
	LC(BObjGravity) = raw(1000_lf);
	LC(WormGravity) = raw(1500_lf);
	LC(JumpForce) = raw(56064_lf);

	LC(MinBounceUp) = raw(-53248_lf);
	LC(MinBounceDown) = raw(53248_lf);
	LC(MinBounceLeft) = raw(-53248_lf);
	LC(MinBounceRight) = raw(53248_lf);
}

void NObjectType::create(State& state, Scalar angle, Vector2 pos, Vector2 vel) const {
	auto* obj = state.nobjects.new_object_reuse();
	obj->ty_idx = this->idx;
	obj->pos = pos;
	obj->vel = vel;

	// TODO: nobjects created with create2 have their velocity applied once immediately

	obj->cell = state.nobject_broadphase.insert(narrow<CellNode::Index>(state.nobjects.index_of(obj)), pos.cast<i32>());

	obj->time_to_die = state.current_time - 1 + this->time_to_live - state.rand.get_unsigned(this->time_to_live_v);

	if (this->start_frame) {
	
		if (this->type == NObjectType::DType1 || this->type == NObjectType::DType2) {
		
#if LIERO_FIXED
			i32 iangle = i32(angle);
#else
			i32 iangle = int(angle * radians_to_liero);
#endif
			
			obj->cur_frame = iangle;
		} else if (this->type == NObjectType::Normal) {
			// TODO: For directional_animation:
			/*
			if(loopAnim)
			{
				if(numFrames)
					obj->curFrame = game.rand(numFrames + 1);
				else
					obj->curFrame = game.rand(2);
			}
			*/

			obj->cur_frame = 0;
		} else {
			// TODO: Other types
			obj->cur_frame = 0;
		}
	} else {
		obj->cur_frame = this->colors[0].v; // TODO: Random
	}
}

void WeaponType::fire(State& state, Scalar angle, Vector2 vel, Vector2 pos) const {

	auto& mod = state.mod;

	/*
	if(w.leaveShells > 0)
	{
		if(game.rand(w.leaveShells) == 0)
		{
			leaveShellTimer = w.leaveShellDelay;
		}
	}
	
	if(w.launchSound >= 0)
	{
		if(w.loopSound)
		{
			if(!game.soundPlayer->isPlaying(&weapons[currentWeapon]))
			{
				game.soundPlayer->play(w.launchSound, &weapons[currentWeapon], -1);
			}
		}
		else
		{
			game.soundPlayer->play(w.launchSound);
		}
	}
	*/

	auto dir = sincos(angle);
	auto anglevel = vel * this->worm_vel_ratio + dir * this->speed;

	pos += dir * i32(this->fire_offset);

	/* TODO: Only apply vel in anglevel if affectByWorm is true
	   TODO: Instead of having affectByWorm, why not have a influence factor. speed decides this in old liero.
	if(w.affectByWorm)
	{
		if(speed < 100)
			speed = 100;
		
		firingVel = vel * 100 / speed;
	}
	*/

	auto& ty = state.mod.get_nobject_type(this->nobject_type);

	u32 lparts = this->parts;
	for (u32 i = 0; i < lparts; ++i) {
		/* TODO: This should probably be done in caller
		if(distribution)
		{
			obj->vel.x += game.rand(distribution * 2) - distribution;
			obj->vel.y += game.rand(distribution * 2) - distribution;
		}
		*/

		auto part_vel = anglevel;

		if (this->distribution != Scalar(0)) {

			part_vel.x += ty.splinter_distribution - rand_max(state.rand, this->distribution * 2);
			part_vel.y += ty.splinter_distribution - rand_max(state.rand, this->distribution * 2);
		}

		ty.create(state, angle, pos, part_vel);
	}
		
}

inline i32 sign(i32 v) {
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

void SObjectType::create(State& state, tl::VectorI2 pos) const {
	SObject* obj = state.sobjects.new_object_reuse();
	obj->pos = tl::VectorI2(pos.x - 8, pos.y - 8);
	obj->ty_idx = this->idx;
	obj->time_to_die = state.current_time + this->anim_delay * (this->num_frames + 1);

	// TODO: Damage and stuff

	//u32 detect_range = 20; // TODO: This is for large_explosion, but it's one less because it's exclusive. Add field.
	Scalar blow_away = 3000_lf; // TODO: This is for large_explosion
	Scalar obj_blow_away = blow_away * Scalar(0.33); // TODO: Store

	if (this->detect_range) {
		u32 ldetect_range = this->detect_range;
	
		auto r = state.nobject_broadphase.area(pos.x - ldetect_range + 1, pos.y - ldetect_range + 1, pos.x + ldetect_range - 1, pos.y + ldetect_range - 1);

		for (u16 nobj_idx; r.next(nobj_idx); ) {
			auto& nobj = state.nobjects.of_index(nobj_idx);

			if (true) { // TODO: affect by explosions in NObjectType
				auto ipos = nobj.pos.cast<i32>();

				if (ipos.x < pos.x + i32(ldetect_range)
				 && ipos.x > pos.x - i32(ldetect_range)
				 && ipos.y < pos.y + i32(ldetect_range)
				 && ipos.y > pos.y - i32(ldetect_range)) {
					auto delta = ipos - pos;
					auto power = tl::VectorI2(ldetect_range - abs(delta.x), ldetect_range - abs(delta.y));

					nobj.vel.x += obj_blow_away * power.x * sign(delta.x);
					nobj.vel.y += obj_blow_away * power.y * sign(delta.y);
				}
			}
		}
	}

	if (this->level_effect) {
		LevelEffect effect;

		u32 tframe_idx = effect.sframe;

		u32 tframe_y = tframe_idx * 16;
		auto tframe = state.mod.large_sprites.crop(tl::RectU(0, tframe_y, 16, tframe_y + 16));

		u32 mframe_y = effect.mframe * 16;
		auto mframe = state.mod.large_sprites.crop(tl::RectU(0, mframe_y, 16, mframe_y + 16));

		tl::ImageSlice targets[2] = { state.level.graphics.slice(), state.level.materials.slice() };
		tl::ImageSlice sources[1] = { mframe };

		tl::BasicBlitContext<2, 1, true> ctx(targets, sources, pos.x - 8, pos.y - 8);

		draw_level_effect(ctx, tframe.pixels);
	}
}

}
