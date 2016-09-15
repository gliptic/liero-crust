#ifndef LIERO_MOD_HPP
#define LIERO_MOD_HPP 1

#include <tl/std.h>
#include <tl/string.hpp>
#include <tl/gfx/image.hpp>
#include <tl/io/filesystem.hpp>
#include <liero-sim/data/model.hpp>
#include <liero-sim/config.hpp>

namespace liero {

struct State;
struct TransientState;

void fire(WeaponType const& self, State& state, TransientState& transient_state, Scalar angle, Vector2 vel, Vector2 pos, i16 owner);

inline u32 computed_loading_time(WeaponType const& self) {
	// TODO: Compute based on settings
	return self.loading_time() * 0; // TEMP: 0% reloading time
}

struct LargeSpriteRow {
	u16 bit0, bit1;
};

struct ModRef {
	TcData const* tcdata;
	WeaponType const* weapon_types;
	NObjectType const* nobject_types;
	SObjectType const* sobject_types;
	LevelEffect const* level_effects;
	u8 const* materials;

	tl::ImageSlice large_sprites, small_sprites, small_font_sprites;
	tl::ImageSlice worm_sprites[2], muzzle_fire_sprites[2];

	tl::VecSlice<LargeSpriteRow> large_sprites_bits;

	tl::VecSlice<tl::Vec<i16>> sounds;

	tl::Palette pal;
	
	NObjectType const& get_nobject_type(u32 index) const {
		return nobject_types[index];
	}

	SObjectType const& get_sobject_type(u32 index) const {
		return sobject_types[index];
	}

	WeaponType const& get_weapon_type(u32 index) const {
		return weapon_types[index];
	}

	LevelEffect const& get_level_effect(u32 index) const {
		return level_effects[index];
	}
};

struct Mod {
	Mod(tl::FsNode& root);

	~Mod() {
	}

	ModRef ref() {
		ModRef ret;
		ret.tcdata = this->tcdata;
		ret.weapon_types = this->weapon_types;
		ret.nobject_types = this->nobject_types;
		ret.sobject_types = this->sobject_types;
		ret.level_effects = this->level_effects;
		ret.large_sprites = this->large_sprites.slice();
		ret.small_sprites = this->small_sprites.slice();
		ret.small_font_sprites = this->small_font_sprites.slice();
		ret.worm_sprites[0] = this->worm_sprites[0].slice();
		ret.worm_sprites[1] = this->worm_sprites[1].slice();
		ret.muzzle_fire_sprites[0] = this->muzzle_fire_sprites[0].slice();
		ret.muzzle_fire_sprites[1] = this->muzzle_fire_sprites[1].slice();
		ret.materials = this->tcdata->materials().begin();

		ret.large_sprites_bits = this->large_sprites_bits.slice();
		ret.sounds = this->sounds.slice();
		ret.pal = this->pal;
		return ret;
	}

	TcData const* tcdata;
	WeaponType const* weapon_types;
	NObjectType const* nobject_types;
	SObjectType const* sobject_types;
	LevelEffect const* level_effects;

	tl::Image large_sprites, small_sprites, small_font_sprites;
	tl::Image worm_sprites[2], muzzle_fire_sprites[2];

	tl::Vec<LargeSpriteRow> large_sprites_bits;

	tl::Vec<tl::Vec<i16>> sounds;
	tl::Palette pal;

	tl::BufferMixed mod_data;
};

}

#endif // LIERO_MOD_HPP
