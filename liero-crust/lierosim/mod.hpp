#ifndef LIERO_MOD_HPP
#define LIERO_MOD_HPP 1

#include "config.hpp"
#include <tl/std.h>
#include <tl/image.hpp>
#include <tl/string.hpp>
#include <tl/filesystem.hpp>

#include "mod_constants.hpp"

namespace liero {

struct State;

struct WeaponType {
	tl::StringSlice name;
	Scalar speed;
	Scalar distribution;
	Scalar worm_vel_ratio; // Original: 100 / max(speed, 100)

	u32 idx;
	u32 parts;
	u32 nobject_type;
	u32 loading_time;
	u32 detect_distance; // TODO: This isn't used and belongs in the NObject
	u32 fire_offset; // (detect_distance + 5) in original

	u32 ammo;
	u32 delay;

	bool play_reload_sound;

	static WeaponType bazooka(i32 bazooka_nobj_type) {
		WeaponType ty;
		ty.name = "BAZOOKA"_S;
		ty.nobject_type = bazooka_nobj_type;
		ty.parts = 1;
		ty.speed = 200_lspeed;
		ty.loading_time = 410;
		ty.ammo = 3;
		ty.delay = 75;
		ty.play_reload_sound = true;
		ty.detect_distance = 1;
		ty.fire_offset = ty.detect_distance + 5;
		ty.distribution = Scalar(0);
		ty.worm_vel_ratio = Scalar(100.0 / 200.0);

		return ty;
	}

	static WeaponType float_mine(u32 float_mine_nobj_type) {
		WeaponType ty;
		ty.name = "FLOAT MINE"_S;
		ty.nobject_type = float_mine_nobj_type;
		ty.parts = 1;
		ty.speed = 40_lspeed;
		ty.loading_time = 280;
		ty.ammo = 2;
		ty.delay = 20;
		ty.play_reload_sound = true;
		ty.detect_distance = 2;
		ty.fire_offset = ty.detect_distance + 5;
		ty.distribution = 14000_lf;
		ty.worm_vel_ratio = Scalar(1);

		return ty;
	}

	static WeaponType fan(u32 fan_nobj_type) {
		WeaponType ty;
		ty.name = "FAN"_S;
		ty.nobject_type = fan_nobj_type;
		ty.parts = 1;
		ty.speed = 180_lspeed;
		ty.loading_time = 220;
		ty.ammo = 150;
		ty.delay = 0;
		ty.play_reload_sound = false;
		ty.detect_distance = 1;
		ty.fire_offset = ty.detect_distance + 5;
		ty.distribution = 12000_lf;
		ty.worm_vel_ratio = Scalar(100.0 / 180.0);

		return ty;
	}

	u32 computed_loading_time() const {
		// TODO: Compute based on settings
		return loading_time;
	}

	void fire(State& state, Scalar angle, Vector2 vel, Vector2 pos) const;
};

struct Mod;

struct NObjectType {
	enum Type : u8 {
		Normal,
		DType1,
		Steerable,
		DType2,
		Laser
	};

	enum ScatterType : u8 {
		ScAngular,
		ScNormal
	};

	u32 idx;
	Scalar gravity;

	u32 splinter_count, splinter_type;
	Scalar splinter_distribution, splinter_speed; // Taken from nobject
	ScatterType splinter_scatter;
	u32 time_to_live, time_to_live_v;

	i32 sobj_trail_type, sobj_trail_delay;
	i32 sobj_expl_type;

	Scalar bounce;
	Scalar friction;
	Scalar drag;
	Scalar blowaway;
	Scalar acceleration;

	i32 start_frame;
	i32 num_frames;
	tl::Color colors[2];

	u32 detect_distance; // TODO: Not used yet

	bool expl_ground;
	bool directional_animation;
	bool sobj_trail_when_hitting; // This should be false for real nobjects, true otherwise
	bool collide_with_objects;
	Type type;

	NObjectType() {
		// This should be set to -Fixed::from_frac(w.bounce, 100)
		bounce = Scalar(-1);
		expl_ground = false;
		detect_distance = 0;
		// This should be set to Fixed::new(1) if this is a wobject and w.bounce == 100, otherwise Fixed::from_frac(4, 5)
		friction = Scalar(1);
		drag = Scalar(1);
		blowaway = Scalar(0);
		acceleration = Scalar(0);
		num_frames = 0;
		directional_animation = true; // This is called loopAnim in original

		splinter_count = 0;
		splinter_type = 0;
		splinter_distribution = Scalar(0);
		splinter_scatter = ScNormal;
		splinter_speed = Scalar(0);

		time_to_live = 0;
		time_to_live_v = 0;
		gravity = Scalar(0);
		colors[0] = tl::Color(0);
		colors[1] = tl::Color(0);
		type = Normal;

		sobj_trail_type = -1;
		sobj_expl_type = -1;
		sobj_trail_when_hitting = false;
		collide_with_objects = false;
	}

	void create(State& state, Scalar angle, Vector2 pos, Vector2 vel) const;

	static NObjectType bazooka(u32 small_damage_nobj_type, i32 hellraider_smoke_type, i32 large_explosion_type);
	static NObjectType float_mine(u32 medium_damage_nobj_type, u32 large_explosion_type);
	static NObjectType fan(Mod& mod);
	static NObjectType small_damage(Mod& mod, i32 small_explosion_type);
	static NObjectType medium_damage(Mod& mod, i32 small_explosion_type);
};

struct SObjectType {
	u32 idx;

	u32 anim_delay;
	u32 start_frame, num_frames; // TODO: num_frames is really one less than the number of frames
	u32 detect_range; // In original, damage decides whether coldet is done on creation, but we use detect_range.
	u32 level_effect;

	void create(State& state, tl::VectorI2 pos) const;

	static SObjectType hellraider_smoke();
	static SObjectType large_explosion();
	static SObjectType small_explosion();
};

struct LevelEffect {
	u32 mframe, rframe, sframe;
	bool draw_back; // TODO: This is the opposide of nDrawBack in the original

	LevelEffect() {
		// TODO: This is for dirt effect 0 (used by large explosion)
		this->mframe = 0;
		this->rframe = 2;
		this->sframe = 73;
		this->draw_back = false;
	}
};

struct Mod {
	Mod();

	WeaponType weapon_types[3];
	NObjectType nobject_types[5];
	SObjectType sobject_types[3];
	tl::Image large_sprites, small_sprites, small_font_sprites;
	tl::Image worm_sprites[2];

	i32 c[MaxC];

	tl::Palette pal;
	
	NObjectType const& get_nobject_type(u32 index) const {
		assert(index < 5);
		return nobject_types[index];
	}

	SObjectType const& get_sobject_type(u32 index) const {
		assert(index < 3);
		return sobject_types[index];
	}

	WeaponType const& get_weapon_type(u32 index) const {
		assert(index < 3);
		return weapon_types[index];
	}
};


inline NObjectType NObjectType::bazooka(u32 small_damage_nobj_type, i32 hellraider_smoke_type, i32 large_explosion_type) {
	NObjectType ty;
	ty.gravity = 0_lf;
	ty.splinter_count = 12;
	ty.splinter_type = small_damage_nobj_type;
	ty.splinter_distribution = 2000_lf;
	ty.splinter_scatter = ScAngular;
	ty.splinter_speed = 160_lspeed;

	ty.sobj_trail_delay = 4;
	ty.sobj_trail_type = hellraider_smoke_type;

	/*
	TODO: Splinter speed, speed_v, distribution: 160lspeed, 140lspeed and 2000lf
	*/
	ty.time_to_live = 0;
	ty.time_to_live_v = 0;
	ty.bounce = Scalar(0);
	ty.friction = Scalar(1);
	ty.acceleration = Scalar(0.03);
		
	ty.start_frame = 2;
	ty.num_frames = 0;
		
	ty.expl_ground = true;
	ty.sobj_expl_type = large_explosion_type;
	ty.sobj_trail_when_hitting = true;
	ty.directional_animation = false;
	ty.type = DType2;
	return ty;
}

inline NObjectType NObjectType::float_mine(u32 medium_damage_nobj_type, u32 large_explosion_type) {
	NObjectType ty;
	ty.gravity = 5_lf;
	ty.splinter_count = 8;
	ty.splinter_type = medium_damage_nobj_type;
	ty.splinter_distribution = 2000_lf;
	ty.splinter_scatter = ScAngular;
	ty.splinter_speed = 190_lspeed; // TODO: Take from medium_damage_nobj_type

	//ty.sobj_trail_delay = 4;
	//ty.sobj_trail_type = hellraider_smoke_type;

	ty.time_to_live = 13000;
	ty.time_to_live_v = 600;
	ty.bounce = Scalar(-1);
	ty.friction = Scalar(1);
	ty.drag = Scalar(0.98);

	ty.start_frame = 68;
	ty.num_frames = 0;
		
	ty.expl_ground = false;
	ty.sobj_expl_type = large_explosion_type;
	ty.sobj_trail_when_hitting = true;
	ty.directional_animation = false;
	ty.type = Normal;
	ty.detect_distance = 2;

	return ty;
}

inline NObjectType NObjectType::fan(Mod& mod) {
	NObjectType ty;
	ty.gravity = 0_lf;
	//ty.splinter_count = 0;
	//ty.splinter_type = medium_damage_nobj_type;
	//ty.splinter_distribution = 2000_lf;
	//ty.splinter_scatter = ScAngular;
	//ty.splinter_speed = 190_lspeed; // TODO: Take from medium_damage_nobj_type

	//ty.sobj_trail_delay = 4;
	//ty.sobj_trail_type = hellraider_smoke_type;

	ty.time_to_live = 45;
	ty.time_to_live_v = 10;
	ty.bounce = Scalar(0);
	ty.friction = Scalar(1);
	ty.blowaway = Scalar(0.3);
	//ty.drag = Scalar(0.98);

	ty.start_frame = 0;
	ty.num_frames = 0;
	ty.colors[0] = mod.pal.entries[25];
	ty.colors[1] = mod.pal.entries[26];
		
	ty.expl_ground = true;
	//ty.sobj_expl_type = 0;
	ty.sobj_trail_when_hitting = true;
	ty.directional_animation = false;
	ty.type = Normal;
	ty.detect_distance = 1;
	ty.collide_with_objects = true;

	return ty;
}

inline NObjectType NObjectType::small_damage(Mod& mod, i32 small_explosion_type) {
	NObjectType ty;
	ty.gravity = 700_lf;
	ty.splinter_count = 0;
	ty.splinter_type = 0;
	ty.time_to_live = 0;
	ty.time_to_live_v = 0;
	ty.bounce = Scalar(0);
	ty.friction = Scalar(1);
		
	ty.start_frame = 0;
	ty.num_frames = 0;
	ty.colors[0] = mod.pal.entries[67];
	ty.colors[1] = mod.pal.entries[66];
		
	ty.expl_ground = true;
	ty.sobj_expl_type = small_explosion_type;
	// TODO: wormDestroy = true
	ty.directional_animation = false;
	ty.type = Normal;
	// TODO: hit damage = 2
	return ty;
}

inline NObjectType NObjectType::medium_damage(Mod& mod, i32 small_explosion_type) {
	NObjectType ty;
	ty.gravity = 700_lf;
	ty.splinter_count = 0;
	ty.splinter_type = 0;
	ty.time_to_live = 0;
	ty.time_to_live_v = 0;
	ty.bounce = Scalar(0);
	ty.friction = Scalar(1);
		
	ty.start_frame = 0;
	ty.num_frames = 0;
	ty.colors[0] = mod.pal.entries[67];
	ty.colors[1] = mod.pal.entries[66];
		
	ty.expl_ground = true;
	ty.sobj_expl_type = small_explosion_type;
	// TODO: wormDestroy = true
	ty.directional_animation = false;
	ty.type = Normal;
	// TODO: hit damage = 3
	return ty;
}

inline SObjectType SObjectType::hellraider_smoke() {
	SObjectType ty;
	ty.anim_delay = 4;
	ty.start_frame = 64;
	ty.num_frames = 4;
	ty.detect_range = 0;
	ty.level_effect = 0;

	return ty;
}

inline SObjectType SObjectType::large_explosion() {
	SObjectType ty;
	ty.anim_delay = 2;
	ty.start_frame = 40;
	ty.num_frames = 15;
	ty.detect_range = 20;
	ty.level_effect = 1;

	return ty;
}

inline SObjectType SObjectType::small_explosion() {
	SObjectType ty;
	ty.anim_delay = 2;
	ty.start_frame = 50;
	ty.num_frames = 5;
	ty.detect_range = 14;
	ty.level_effect = 0; // TODO: This is wrong

	return ty;
}

}

#endif // LIERO_MOD_HPP
