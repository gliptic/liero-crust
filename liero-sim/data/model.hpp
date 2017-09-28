#ifndef LIEROSIM_MODEL_HPP
#define LIEROSIM_MODEL_HPP 1

#include <tl/std.h>
#include <tl/bits.h>
#include "base_model.hpp"

#include <tl/gfx/image.hpp>

namespace liero {

struct WeaponType;
struct WeaponTypeList;
struct NObjectType;
struct SObjectType;
struct NObjectEmitterType;
struct LevelEffect;
struct TcData;
struct PlayerControls;
struct PlayerSettings;

enum struct NObjectKind : u8 {
	Normal = 0,
	DType1 = 1,
	Steerable = 2,
	DType2 = 3,
	Laser = 4,
};

enum struct ScatterType : u8 {
	ScAngular = 0,
	ScNormal = 1,
};

enum struct NObjectAnimation : u8 {
	TwoWay = 0,
	OneWay = 1,
	Static = 2,
};

struct alignas(8) WeaponTypeReader : ss::Struct {
	u8 data[72];

};

struct alignas(8) WeaponType : ss::Struct {
	static u8 _defaults[72];

	u8 data[72];

	WeaponType() { memcpy(data, _defaults, sizeof(_defaults)); }

	tl::StringSlice name() const { return this->_field<ss::StringOffset, 0>().get(); }
	f64 speed() const { return this->_field<f64, 8>(); }
	f64 distribution() const { return this->_field<f64, 16>(); }
	f64 worm_vel_ratio() const { return this->_field<f64, 24>(); }
	u32 parts() const { return this->_field<u32, 32>(); }
	u16 nobject_type() const { return this->_field<u16, 36>(); }
	u32 loading_time() const { return this->_field<u32, 40>(); }
	u32 fire_offset() const { return this->_field<u32, 44>(); }
	u32 ammo() const { return this->_field<u32, 48>(); }
	u32 delay() const { return this->_field<u32, 52>(); }
	bool play_reload_sound() const { return (this->_field<u8, 38>() & 1) != 0; }
	f64 recoil() const { return this->_field<f64, 56>(); }
	i16 fire_sound() const { return this->_field<i16, 64>(); }
	u8 muzzle_fire() const { return this->_field<u8, 39>(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<WeaponTypeReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		WeaponTypeReader const* srcp = src.get();
		cur_size += ss::round_size_up(srcp->_field<ss::StringOffset, 0>().size);
		return cur_size;
	}

	static void expand_raw(ss::Ref<WeaponType> dest, ss::Expander& expander, ss::StructOffset<WeaponTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		WeaponTypeReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)WeaponType::_defaults;
		for (u32 i = 0; i < 9; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto name_copy = expander.alloc_str_raw(srcp->_field<ss::StringOffset, 0>().get());
		dest._field_ref<ss::StringOffset, 0>().set(name_copy);
	}

};

struct WeaponTypeBuilder : ss::Ref<WeaponTypeReader> {
	WeaponTypeBuilder(ss::Builder& b)
		: Ref<WeaponTypeReader>(b.alloc<WeaponTypeReader>()) {
	}

	Ref<WeaponTypeReader> done() { return std::move(*this); }

	void name(ss::StringRef v) { return this->_field_ref<ss::StringOffset, 0>().set(v); }

	void speed(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 8>() = s ^ 0;
	}

	void distribution(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 16>() = s ^ 0;
	}

	void worm_vel_ratio(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 24>() = s ^ 0;
	}
	void parts(u32 v) { this->_field<u32, 32>() = v ^ 0; }
	void nobject_type(u16 v) { this->_field<u16, 36>() = v ^ 0; }
	void loading_time(u32 v) { this->_field<u32, 40>() = v ^ 0; }
	void fire_offset(u32 v) { this->_field<u32, 44>() = v ^ 0; }
	void ammo(u32 v) { this->_field<u32, 48>() = v ^ 0; }
	void delay(u32 v) { this->_field<u32, 52>() = v ^ 0; }
	void play_reload_sound(bool v) { this->_field<u8, 38>() = (this->_field<u8, 38>() & 0xfe) | ((v ^ 0) << 0); }

	void recoil(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 56>() = s ^ 0;
	}
	void fire_sound(i16 v) { this->_field<i16, 64>() = v ^ 0; }
	void muzzle_fire(u8 v) { this->_field<u8, 39>() = v ^ 0; }

};

struct alignas(8) WeaponTypeListReader : ss::Struct {
	u8 data[8];

};

struct alignas(8) WeaponTypeList : ss::Struct {
	static u8 _defaults[8];

	u8 data[8];

	WeaponTypeList() { memcpy(data, _defaults, sizeof(_defaults)); }

	tl::VecSlice<WeaponType const> list() const { return this->_field<ss::ArrayOffset<WeaponType>, 0>().get(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<WeaponTypeListReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		WeaponTypeListReader const* srcp = src.get();
		cur_size = expander.array_calc_size<WeaponType, ss::StructOffset<WeaponTypeReader>>(cur_size, srcp->_field<ss::Offset, 0>());
		return cur_size;
	}

	static void expand_raw(ss::Ref<WeaponTypeList> dest, ss::Expander& expander, ss::StructOffset<WeaponTypeListReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		WeaponTypeListReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)WeaponTypeList::_defaults;
		for (u32 i = 0; i < 1; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto list_copy = expander.expand_array_raw<WeaponType, ss::StructOffset<WeaponTypeReader>>(srcp->_field<ss::Offset, 0>());
		dest._field_ref<ss::ArrayOffset<WeaponType>, 0>().set(list_copy);
	}

};

struct WeaponTypeListBuilder : ss::Ref<WeaponTypeListReader> {
	WeaponTypeListBuilder(ss::Builder& b)
		: Ref<WeaponTypeListReader>(b.alloc<WeaponTypeListReader>()) {
	}

	Ref<WeaponTypeListReader> done() { return std::move(*this); }

	void list(ss::ArrayRef<ss::StructOffset<WeaponTypeReader>> v) { return this->_field_ref<ss::ArrayOffset<ss::StructOffset<WeaponTypeReader>>, 0>().set(v); }

};

struct alignas(8) NObjectTypeReader : ss::Struct {
	u8 data[176];

};

struct alignas(8) NObjectType : ss::Struct {
	static u8 _defaults[176];

	u8 data[176];

	NObjectType() { memcpy(data, _defaults, sizeof(_defaults)); }

	Scalar gravity() const { return this->_field<Scalar, 0>(); }
	u32 splinter_count() const { return this->_field<u32, 4>(); }
	u16 splinter_type() const { return this->_field<u16, 8>(); }
	ScatterType splinter_scatter() const { return this->_field<ScatterType, 10>(); }
	f64 splinter_distribution() const { return this->_field<f64, 16>(); }
	f64 splinter_speed_v() const { return this->_field<f64, 24>(); }
	u32 time_to_live() const { return this->_field<u32, 12>(); }
	i32 time_to_live_v() const { return this->_field<i32, 32>(); }
	u16 sobj_trail_type() const { return this->_field<u16, 36>(); }
	u32 sobj_trail_interval() const { return this->_field<u32, 40>(); }
	i16 sobj_expl_type() const { return this->_field<i16, 38>(); }
	f64 bounce() const { return this->_field<f64, 48>(); }
	f64 friction() const { return this->_field<f64, 56>(); }
	f64 drag() const { return this->_field<f64, 64>(); }
	f64 blowaway() const { return this->_field<f64, 72>(); }
	f64 acceleration() const { return this->_field<f64, 80>(); }
	i32 start_frame() const { return this->_field<i32, 44>(); }
	i32 num_frames() const { return this->_field<i32, 88>(); }	typedef tl::Color colorsVal[2];
	colorsVal const& colors() const { return this->_field<colorsVal const, 92>(); }

	u32 detect_distance() const { return this->_field<u32, 100>(); }
	bool expl_ground() const { return (this->_field<u8, 11>() & 1) != 0; }
	bool sobj_trail_when_hitting() const { return (this->_field<u8, 11>() & 2) != 0; }
	bool collide_with_objects() const { return (this->_field<u8, 11>() & 4) != 0; }
	NObjectKind kind() const { return this->_field<NObjectKind, 104>(); }
	u32 blood_trail_interval() const { return this->_field<u32, 108>(); }
	u32 nobj_trail_interval() const { return this->_field<u32, 112>(); }
	u16 nobj_trail_type() const { return this->_field<u16, 106>(); }
	ScatterType nobj_trail_scatter() const { return this->_field<ScatterType, 105>(); }
	f64 nobj_trail_vel_ratio() const { return this->_field<f64, 120>(); }
	f64 nobj_trail_speed() const { return this->_field<f64, 128>(); }
	u8 physics_speed() const { return this->_field<u8, 116>(); }
	tl::Color splinter_color() const { return this->_field<tl::Color, 136>(); }
	bool sobj_trail_when_bounced() const { return (this->_field<u8, 11>() & 8) != 0; }
	NObjectAnimation animation() const { return this->_field<NObjectAnimation, 117>(); }
	u16 hit_damage() const { return this->_field<u16, 118>(); }
	i16 level_effect() const { return this->_field<i16, 140>(); }
	bool affect_by_sobj() const { return (this->_field<u8, 11>() & 16) != 0; }
	bool draw_on_level() const { return (this->_field<u8, 11>() & 32) != 0; }
	f64 acceleration_up() const { return this->_field<f64, 144>(); }
	i16 expl_sound() const { return this->_field<i16, 142>(); }
	bool worm_coldet() const { return (this->_field<u8, 11>() & 64) != 0; }
	u32 worm_col_remove_prob() const { return this->_field<u32, 152>(); }
	bool worm_col_expl() const { return (this->_field<u8, 11>() & 128) != 0; }
	u32 worm_col_blood() const { return this->_field<u32, 156>(); }
	u32 nobj_trail_interval_inv() const { return this->_field<u32, 160>(); }
	f64 splinter_speed() const { return this->_field<f64, 168>(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<NObjectTypeReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		return cur_size;
	}

	static void expand_raw(ss::Ref<NObjectType> dest, ss::Expander& expander, ss::StructOffset<NObjectTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		NObjectTypeReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)NObjectType::_defaults;
		for (u32 i = 0; i < 22; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}
};

struct NObjectTypeBuilder : ss::Ref<NObjectTypeReader> {
	NObjectTypeBuilder(ss::Builder& b)
		: Ref<NObjectTypeReader>(b.alloc<NObjectTypeReader>()) {
	}

	Ref<NObjectTypeReader> done() { return std::move(*this); }

	void gravity(Scalar v) { this->_field<i32, 0>() = (v.raw()) ^ 0; }
	void splinter_count(u32 v) { this->_field<u32, 4>() = v ^ 0; }
	void splinter_type(u16 v) { this->_field<u16, 8>() = v ^ 0; }
	void splinter_scatter(ScatterType v) { this->_field<u8, 10>() = u8(v) ^ 1; }

	void splinter_distribution(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 16>() = s ^ 0;
	}

	void splinter_speed_v(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 24>() = s ^ 0;
	}
	void time_to_live(u32 v) { this->_field<u32, 12>() = v ^ 0; }
	void time_to_live_v(i32 v) { this->_field<i32, 32>() = v ^ 0; }
	void sobj_trail_type(u16 v) { this->_field<u16, 36>() = v ^ 0; }
	void sobj_trail_interval(u32 v) { this->_field<u32, 40>() = v ^ 0; }
	void sobj_expl_type(i16 v) { this->_field<i16, 38>() = v ^ 65535; }

	void bounce(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 48>() = s ^ -4616189618054758400;
	}

	void friction(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 56>() = s ^ 4607182418800017408;
	}

	void drag(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 64>() = s ^ 4607182418800017408;
	}

	void blowaway(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 72>() = s ^ 0;
	}

	void acceleration(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 80>() = s ^ 0;
	}
	void start_frame(i32 v) { this->_field<i32, 44>() = v ^ 0; }
	void num_frames(i32 v) { this->_field<i32, 88>() = v ^ 0; }
	typedef tl::Color colorsVal[2];
	colorsVal& colors() { return this->_field<colorsVal, 92>(); }
	void detect_distance(u32 v) { this->_field<u32, 100>() = v ^ 0; }
	void expl_ground(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xfe) | ((v ^ 0) << 0); }
	void sobj_trail_when_hitting(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xfd) | ((v ^ 0) << 1); }
	void collide_with_objects(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xfb) | ((v ^ 0) << 2); }
	void kind(NObjectKind v) { this->_field<u8, 104>() = u8(v) ^ 0; }
	void blood_trail_interval(u32 v) { this->_field<u32, 108>() = v ^ 0; }
	void nobj_trail_interval(u32 v) { this->_field<u32, 112>() = v ^ 0; }
	void nobj_trail_type(u16 v) { this->_field<u16, 106>() = v ^ 0; }
	void nobj_trail_scatter(ScatterType v) { this->_field<u8, 105>() = u8(v) ^ 1; }

	void nobj_trail_vel_ratio(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 120>() = s ^ 4599676419421066581;
	}

	void nobj_trail_speed(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 128>() = s ^ 0;
	}
	void physics_speed(u8 v) { this->_field<u8, 116>() = v ^ 1; }
	void splinter_color(tl::Color v) { this->_field<u32, 136>() = (v.v) ^ 0; }
	void sobj_trail_when_bounced(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xf7) | ((v ^ 0) << 3); }
	void animation(NObjectAnimation v) { this->_field<u8, 117>() = u8(v) ^ 0; }
	void hit_damage(u16 v) { this->_field<u16, 118>() = v ^ 0; }
	void level_effect(i16 v) { this->_field<i16, 140>() = v ^ 65535; }
	void affect_by_sobj(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xef) | ((v ^ 0) << 4); }
	void draw_on_level(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xdf) | ((v ^ 0) << 5); }

	void acceleration_up(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 144>() = s ^ 0;
	}
	void expl_sound(i16 v) { this->_field<i16, 142>() = v ^ 65535; }
	void worm_coldet(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0xbf) | ((v ^ 0) << 6); }
	void worm_col_remove_prob(u32 v) { this->_field<u32, 152>() = v ^ 0; }
	void worm_col_expl(bool v) { this->_field<u8, 11>() = (this->_field<u8, 11>() & 0x7f) | ((v ^ 0) << 7); }
	void worm_col_blood(u32 v) { this->_field<u32, 156>() = v ^ 0; }
	void nobj_trail_interval_inv(u32 v) { this->_field<u32, 160>() = v ^ 0; }

	void splinter_speed(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 168>() = s ^ 0;
	}

};

struct alignas(8) SObjectTypeReader : ss::Struct {
	u8 data[32];

};

struct alignas(8) SObjectType : ss::Struct {
	static u8 _defaults[32];

	u8 data[32];

	SObjectType() { memcpy(data, _defaults, sizeof(_defaults)); }

	u32 anim_delay() const { return this->_field<u32, 0>(); }
	u16 start_frame() const { return this->_field<u16, 4>(); }
	u16 num_frames() const { return this->_field<u16, 6>(); }
	u32 detect_range() const { return this->_field<u32, 8>(); }
	i16 level_effect() const { return this->_field<i16, 12>(); }
	i16 start_sound() const { return this->_field<i16, 14>(); }
	u8 num_sounds() const { return this->_field<u8, 16>(); }
	Scalar worm_blow_away() const { return this->_field<Scalar, 20>(); }
	Scalar nobj_blow_away() const { return this->_field<Scalar, 24>(); }
	u32 damage() const { return this->_field<u32, 28>(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<SObjectTypeReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		return cur_size;
	}

	static void expand_raw(ss::Ref<SObjectType> dest, ss::Expander& expander, ss::StructOffset<SObjectTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		SObjectTypeReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)SObjectType::_defaults;
		for (u32 i = 0; i < 4; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}

};

struct SObjectTypeBuilder : ss::Ref<SObjectTypeReader> {
	SObjectTypeBuilder(ss::Builder& b)
		: Ref<SObjectTypeReader>(b.alloc<SObjectTypeReader>()) {
	}

	Ref<SObjectTypeReader> done() { return std::move(*this); }

	void anim_delay(u32 v) { this->_field<u32, 0>() = v ^ 0; }
	void start_frame(u16 v) { this->_field<u16, 4>() = v ^ 0; }
	void num_frames(u16 v) { this->_field<u16, 6>() = v ^ 0; }
	void detect_range(u32 v) { this->_field<u32, 8>() = v ^ 0; }
	void level_effect(i16 v) { this->_field<i16, 12>() = v ^ 65535; }
	void start_sound(i16 v) { this->_field<i16, 14>() = v ^ 65535; }
	void num_sounds(u8 v) { this->_field<u8, 16>() = v ^ 0; }
	void worm_blow_away(Scalar v) { this->_field<i32, 20>() = (v.raw()) ^ 0; }
	void nobj_blow_away(Scalar v) { this->_field<i32, 24>() = (v.raw()) ^ 0; }
	void damage(u32 v) { this->_field<u32, 28>() = v ^ 0; }

};

struct alignas(8) NObjectEmitterTypeReader : ss::Struct {
	u8 data[24];

};

struct alignas(8) NObjectEmitterType : ss::Struct {
	static u8 _defaults[24];

	u8 data[24];

	NObjectEmitterType() { memcpy(data, _defaults, sizeof(_defaults)); }

	f64 speed() const { return this->_field<f64, 0>(); }
	f64 speed_v() const { return this->_field<f64, 8>(); }
	f64 distribution() const { return this->_field<f64, 16>(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<NObjectEmitterTypeReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		return cur_size;
	}

	static void expand_raw(ss::Ref<NObjectEmitterType> dest, ss::Expander& expander, ss::StructOffset<NObjectEmitterTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		NObjectEmitterTypeReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)NObjectEmitterType::_defaults;
		for (u32 i = 0; i < 3; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}

};

struct NObjectEmitterTypeBuilder : ss::Ref<NObjectEmitterTypeReader> {
	NObjectEmitterTypeBuilder(ss::Builder& b)
		: Ref<NObjectEmitterTypeReader>(b.alloc<NObjectEmitterTypeReader>()) {
	}

	Ref<NObjectEmitterTypeReader> done() { return std::move(*this); }


	void speed(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 0>() = s ^ 0;
	}

	void speed_v(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 8>() = s ^ 0;
	}

	void distribution(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 16>() = s ^ 0;
	}

};

struct alignas(8) LevelEffectReader : ss::Struct {
	u8 data[16];

};

struct alignas(8) LevelEffect : ss::Struct {
	static u8 _defaults[16];

	u8 data[16];

	LevelEffect() { memcpy(data, _defaults, sizeof(_defaults)); }

	u32 mframe() const { return this->_field<u32, 0>(); }
	u32 rframe() const { return this->_field<u32, 4>(); }
	u32 sframe() const { return this->_field<u32, 8>(); }
	bool draw_back() const { return (this->_field<u8, 12>() & 1) != 0; }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<LevelEffectReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		return cur_size;
	}

	static void expand_raw(ss::Ref<LevelEffect> dest, ss::Expander& expander, ss::StructOffset<LevelEffectReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		LevelEffectReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)LevelEffect::_defaults;
		for (u32 i = 0; i < 2; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}

};

struct LevelEffectBuilder : ss::Ref<LevelEffectReader> {
	LevelEffectBuilder(ss::Builder& b)
		: Ref<LevelEffectReader>(b.alloc<LevelEffectReader>()) {
	}

	Ref<LevelEffectReader> done() { return std::move(*this); }

	void mframe(u32 v) { this->_field<u32, 0>() = v ^ 0; }
	void rframe(u32 v) { this->_field<u32, 4>() = v ^ 0; }
	void sframe(u32 v) { this->_field<u32, 8>() = v ^ 0; }
	void draw_back(bool v) { this->_field<u8, 12>() = (this->_field<u8, 12>() & 0xfe) | ((v ^ 0) << 0); }

};

struct alignas(8) TcDataReader : ss::Struct {
	u8 data[296];

};

struct alignas(8) TcData : ss::Struct {
	static u8 _defaults[296];

	u8 data[296];

	TcData() { memcpy(data, _defaults, sizeof(_defaults)); }

	tl::VecSlice<NObjectType const> nobjects() const { return this->_field<ss::ArrayOffset<NObjectType>, 0>().get(); }
	tl::VecSlice<SObjectType const> sobjects() const { return this->_field<ss::ArrayOffset<SObjectType>, 8>().get(); }
	tl::VecSlice<WeaponType const> weapons() const { return this->_field<ss::ArrayOffset<WeaponType>, 16>().get(); }
	tl::VecSlice<LevelEffect const> level_effects() const { return this->_field<ss::ArrayOffset<LevelEffect>, 24>().get(); }
	f64 nr_initial_length() const { return this->_field<f64, 32>(); }
	f64 nr_attach_length() const { return this->_field<f64, 40>(); }
	f64 nr_min_length() const { return this->_field<f64, 48>(); }
	f64 nr_max_length() const { return this->_field<f64, 56>(); }
	f64 nr_throw_vel() const { return this->_field<f64, 64>(); }
	f64 nr_force_mult() const { return this->_field<f64, 72>(); }
	f64 nr_pull_vel() const { return this->_field<f64, 80>(); }
	f64 nr_release_vel() const { return this->_field<f64, 88>(); }
	u8 nr_colour_begin() const { return this->_field<u8, 96>(); }
	u8 nr_colour_end() const { return this->_field<u8, 97>(); }
	Scalar min_bounce() const { return this->_field<Scalar, 100>(); }
	Scalar worm_gravity() const { return this->_field<Scalar, 104>(); }
	Scalar walk_vel() const { return this->_field<Scalar, 108>(); }
	Scalar max_vel() const { return this->_field<Scalar, 112>(); }
	Scalar jump_force() const { return this->_field<Scalar, 116>(); }
	Scalar max_aim_vel() const { return this->_field<Scalar, 120>(); }
	Scalar aim_acc() const { return this->_field<Scalar, 124>(); }
	Scalar ninjarope_gravity() const { return this->_field<Scalar, 128>(); }
	Scalar bonus_gravity() const { return this->_field<Scalar, 132>(); }
	f64 worm_fric_mult() const { return this->_field<f64, 136>(); }
	u32 worm_min_spawn_dist_last() const { return this->_field<u32, 144>(); }
	u32 worm_min_spawn_dist_enemy() const { return this->_field<u32, 148>(); }
	f64 aim_fric_mult() const { return this->_field<f64, 152>(); }
	f64 bonus_bounce_mult() const { return this->_field<f64, 160>(); }
	u32 bonus_flicker_time() const { return this->_field<u32, 168>(); }
	Scalar bonus_explode_risk() const { return this->_field<Scalar, 172>(); }
	u32 bonus_health_var() const { return this->_field<u32, 176>(); }
	u32 bonus_min_health() const { return this->_field<u32, 180>(); }
	f64 bonus_drop_chance() const { return this->_field<f64, 184>(); }
	u32 aim_max_right() const { return this->_field<u32, 192>(); }
	u32 aim_min_right() const { return this->_field<u32, 196>(); }
	u32 aim_max_left() const { return this->_field<u32, 200>(); }
	u32 aim_min_left() const { return this->_field<u32, 204>(); }
	u8 first_blood_colour() const { return this->_field<u8, 98>(); }
	u8 num_blood_colours() const { return this->_field<u8, 99>(); }
	Scalar bobj_gravity() const { return this->_field<Scalar, 208>(); }
	u32 blood_step_up() const { return this->_field<u32, 212>(); }
	u32 blood_step_down() const { return this->_field<u32, 216>(); }
	u32 blood_limit() const { return this->_field<u32, 220>(); }
	u32 fall_damage_right() const { return this->_field<u32, 224>(); }
	u32 fall_damage_left() const { return this->_field<u32, 228>(); }
	u32 fall_damage_down() const { return this->_field<u32, 232>(); }
	u32 fall_damage_up() const { return this->_field<u32, 236>(); }
	u32 worm_float_level() const { return this->_field<u32, 240>(); }
	Scalar worm_float_power() const { return this->_field<Scalar, 244>(); }
	i16 rem_exp_object() const { return this->_field<i16, 248>(); }
	tl::VecSlice<u8 const> materials() const { return this->_field<ss::ArrayOffset<u8>, 256>().get(); }
	tl::VecSlice<ss::StringOffset const> sound_names() const { return this->_field<ss::ArrayOffset<ss::StringOffset>, 264>().get(); }
	u8 throw_sound() const { return this->_field<u8, 250>(); }	typedef u16 bonus_sobjVal[2];
	bonus_sobjVal const& bonus_sobj() const { return this->_field<bonus_sobjVal const, 252>(); }
	typedef u16 bonus_rand_timer_minVal[2];
	bonus_rand_timer_minVal const& bonus_rand_timer_min() const { return this->_field<bonus_rand_timer_minVal const, 272>(); }
	typedef u16 bonus_rand_timer_varVal[2];
	bonus_rand_timer_varVal const& bonus_rand_timer_var() const { return this->_field<bonus_rand_timer_varVal const, 276>(); }
	typedef u16 bonus_framesVal[2];
	bonus_framesVal const& bonus_frames() const { return this->_field<bonus_framesVal const, 280>(); }

	u8 reload_sound() const { return this->_field<u8, 251>(); }
	NObjectEmitterType const& blood_emitter() const { return *this->_field<ss::StructOffset<NObjectEmitterType>, 288>().get(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<TcDataReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		TcDataReader const* srcp = src.get();
		cur_size = expander.array_calc_size<NObjectType, ss::StructOffset<NObjectTypeReader>>(cur_size, srcp->_field<ss::Offset, 0>());
		cur_size = expander.array_calc_size<SObjectType, ss::StructOffset<SObjectTypeReader>>(cur_size, srcp->_field<ss::Offset, 8>());
		cur_size = expander.array_calc_size<WeaponType, ss::StructOffset<WeaponTypeReader>>(cur_size, srcp->_field<ss::Offset, 16>());
		cur_size = expander.array_calc_size<LevelEffect, ss::StructOffset<LevelEffectReader>>(cur_size, srcp->_field<ss::Offset, 24>());
		cur_size = expander.array_calc_size_plain<u8, u8>(cur_size, srcp->_field<ss::Offset, 256>());
		cur_size = expander.array_calc_size<ss::StringOffset, ss::StringOffset>(cur_size, srcp->_field<ss::Offset, 264>());
		cur_size = NObjectEmitterType::calc_extra_size(24 + cur_size, expander, srcp->_field<ss::StructOffset<NObjectEmitterTypeReader> const, 288>());
		return cur_size;
	}

	static void expand_raw(ss::Ref<TcData> dest, ss::Expander& expander, ss::StructOffset<TcDataReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		TcDataReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)TcData::_defaults;
		for (u32 i = 0; i < 37; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto nobjects_copy = expander.expand_array_raw<NObjectType, ss::StructOffset<NObjectTypeReader>>(srcp->_field<ss::Offset, 0>());
		auto sobjects_copy = expander.expand_array_raw<SObjectType, ss::StructOffset<SObjectTypeReader>>(srcp->_field<ss::Offset, 8>());
		auto weapons_copy = expander.expand_array_raw<WeaponType, ss::StructOffset<WeaponTypeReader>>(srcp->_field<ss::Offset, 16>());
		auto level_effects_copy = expander.expand_array_raw<LevelEffect, ss::StructOffset<LevelEffectReader>>(srcp->_field<ss::Offset, 24>());
		auto materials_copy = expander.expand_array_raw_plain<u8, u8>(srcp->_field<ss::Offset, 256>());
		auto sound_names_copy = expander.expand_array_raw<ss::StringOffset, ss::StringOffset>(srcp->_field<ss::Offset, 264>());
		auto blood_emitter_copy = expander.alloc_uninit_raw<NObjectEmitterType>();
		NObjectEmitterType::expand_raw(blood_emitter_copy, expander, srcp->_field<ss::StructOffset<NObjectEmitterTypeReader>, 288>());
		dest._field_ref<ss::ArrayOffset<NObjectType>, 0>().set(nobjects_copy);
		dest._field_ref<ss::ArrayOffset<SObjectType>, 8>().set(sobjects_copy);
		dest._field_ref<ss::ArrayOffset<WeaponType>, 16>().set(weapons_copy);
		dest._field_ref<ss::ArrayOffset<LevelEffect>, 24>().set(level_effects_copy);
		dest._field_ref<ss::ArrayOffset<u8>, 256>().set(materials_copy);
		dest._field_ref<ss::ArrayOffset<ss::StringOffset>, 264>().set(sound_names_copy);
		dest._field_ref<ss::StructOffset<NObjectEmitterType>, 288>().set(blood_emitter_copy);
	}
};

struct TcDataBuilder : ss::Ref<TcDataReader> {
	TcDataBuilder(ss::Builder& b)
		: Ref<TcDataReader>(b.alloc<TcDataReader>()) {
	}

	Ref<TcDataReader> done() { return std::move(*this); }

	void nobjects(ss::ArrayRef<ss::StructOffset<NObjectTypeReader>> v) { return this->_field_ref<ss::ArrayOffset<ss::StructOffset<NObjectTypeReader>>, 0>().set(v); }
	void sobjects(ss::ArrayRef<ss::StructOffset<SObjectTypeReader>> v) { return this->_field_ref<ss::ArrayOffset<ss::StructOffset<SObjectTypeReader>>, 8>().set(v); }
	void weapons(ss::ArrayRef<ss::StructOffset<WeaponTypeReader>> v) { return this->_field_ref<ss::ArrayOffset<ss::StructOffset<WeaponTypeReader>>, 16>().set(v); }
	void level_effects(ss::ArrayRef<ss::StructOffset<LevelEffectReader>> v) { return this->_field_ref<ss::ArrayOffset<ss::StructOffset<LevelEffectReader>>, 24>().set(v); }

	void nr_initial_length(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 32>() = s ^ 4643000109586448384;
	}

	void nr_attach_length(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 40>() = s ^ 4628609701402116096;
	}

	void nr_min_length(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 48>() = s ^ 4622170961309859840;
	}

	void nr_max_length(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 56>() = s ^ 4643000109586448384;
	}

	void nr_throw_vel(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 64>() = s ^ 4616189618054758400;
	}

	void nr_force_mult(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 72>() = s ^ 0;
	}

	void nr_pull_vel(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 80>() = s ^ 0;
	}

	void nr_release_vel(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 88>() = s ^ 0;
	}
	void nr_colour_begin(u8 v) { this->_field<u8, 96>() = v ^ 0; }
	void nr_colour_end(u8 v) { this->_field<u8, 97>() = v ^ 0; }
	void min_bounce(Scalar v) { this->_field<i32, 100>() = (v.raw()) ^ 53248; }
	void worm_gravity(Scalar v) { this->_field<i32, 104>() = (v.raw()) ^ 1500; }
	void walk_vel(Scalar v) { this->_field<i32, 108>() = (v.raw()) ^ 3000; }
	void max_vel(Scalar v) { this->_field<i32, 112>() = (v.raw()) ^ 29184; }
	void jump_force(Scalar v) { this->_field<i32, 116>() = (v.raw()) ^ 56064; }
	void max_aim_vel(Scalar v) { this->_field<i32, 120>() = (v.raw()) ^ 70000; }
	void aim_acc(Scalar v) { this->_field<i32, 124>() = (v.raw()) ^ 4000; }
	void ninjarope_gravity(Scalar v) { this->_field<i32, 128>() = (v.raw()) ^ 0; }
	void bonus_gravity(Scalar v) { this->_field<i32, 132>() = (v.raw()) ^ 0; }

	void worm_fric_mult(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 136>() = s ^ 4606191621384437760;
	}
	void worm_min_spawn_dist_last(u32 v) { this->_field<u32, 144>() = v ^ 0; }
	void worm_min_spawn_dist_enemy(u32 v) { this->_field<u32, 148>() = v ^ 0; }

	void aim_fric_mult(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 152>() = s ^ 4605651073980432384;
	}

	void bonus_bounce_mult(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 160>() = s ^ 0;
	}
	void bonus_flicker_time(u32 v) { this->_field<u32, 168>() = v ^ 0; }
	void bonus_explode_risk(Scalar v) { this->_field<i32, 172>() = (v.raw()) ^ 0; }
	void bonus_health_var(u32 v) { this->_field<u32, 176>() = v ^ 0; }
	void bonus_min_health(u32 v) { this->_field<u32, 180>() = v ^ 0; }

	void bonus_drop_chance(f64 v) {
		u64 s;
		memcpy(&s, &v, sizeof(v));
		this->_field<u64, 184>() = s ^ 0;
	}
	void aim_max_right(u32 v) { this->_field<u32, 192>() = v ^ 116; }
	void aim_min_right(u32 v) { this->_field<u32, 196>() = v ^ 64; }
	void aim_max_left(u32 v) { this->_field<u32, 200>() = v ^ 12; }
	void aim_min_left(u32 v) { this->_field<u32, 204>() = v ^ 64; }
	void first_blood_colour(u8 v) { this->_field<u8, 98>() = v ^ 80; }
	void num_blood_colours(u8 v) { this->_field<u8, 99>() = v ^ 2; }
	void bobj_gravity(Scalar v) { this->_field<i32, 208>() = (v.raw()) ^ 1000; }
	void blood_step_up(u32 v) { this->_field<u32, 212>() = v ^ 0; }
	void blood_step_down(u32 v) { this->_field<u32, 216>() = v ^ 0; }
	void blood_limit(u32 v) { this->_field<u32, 220>() = v ^ 0; }
	void fall_damage_right(u32 v) { this->_field<u32, 224>() = v ^ 0; }
	void fall_damage_left(u32 v) { this->_field<u32, 228>() = v ^ 0; }
	void fall_damage_down(u32 v) { this->_field<u32, 232>() = v ^ 0; }
	void fall_damage_up(u32 v) { this->_field<u32, 236>() = v ^ 0; }
	void worm_float_level(u32 v) { this->_field<u32, 240>() = v ^ 0; }
	void worm_float_power(Scalar v) { this->_field<i32, 244>() = (v.raw()) ^ 0; }
	void rem_exp_object(i16 v) { this->_field<i16, 248>() = v ^ 0; }
	void materials(ss::ArrayRef<u8> v) { return this->_field_ref<ss::ArrayOffset<u8>, 256>().set(v); }
	void sound_names(ss::ArrayRef<ss::StringOffset> v) { return this->_field_ref<ss::ArrayOffset<ss::StringOffset>, 264>().set(v); }
	void throw_sound(u8 v) { this->_field<u8, 250>() = v ^ 0; }
	typedef u16 bonus_sobjVal[2];
	bonus_sobjVal& bonus_sobj() { return this->_field<bonus_sobjVal, 252>(); }
	typedef u16 bonus_rand_timer_minVal[2];
	bonus_rand_timer_minVal& bonus_rand_timer_min() { return this->_field<bonus_rand_timer_minVal, 272>(); }
	typedef u16 bonus_rand_timer_varVal[2];
	bonus_rand_timer_varVal& bonus_rand_timer_var() { return this->_field<bonus_rand_timer_varVal, 276>(); }
	typedef u16 bonus_framesVal[2];
	bonus_framesVal& bonus_frames() { return this->_field<bonus_framesVal, 280>(); }
	void reload_sound(u8 v) { this->_field<u8, 251>() = v ^ 0; }
	void blood_emitter(ss::Ref<NObjectEmitterTypeReader> v) { return this->_field_ref<ss::StructOffset<NObjectEmitterTypeReader>, 288>().set(v); }

};

struct alignas(8) PlayerControlsReader : ss::Struct {
	u8 data[16];

};

struct alignas(8) PlayerControls : ss::Struct {
	static u8 _defaults[16];

	u8 data[16];

	PlayerControls() { memcpy(data, _defaults, sizeof(_defaults)); }

	u16 up() const { return this->_field<u16, 0>(); }
	u16 down() const { return this->_field<u16, 2>(); }
	u16 left() const { return this->_field<u16, 4>(); }
	u16 right() const { return this->_field<u16, 6>(); }
	u16 fire() const { return this->_field<u16, 8>(); }
	u16 change() const { return this->_field<u16, 10>(); }
	u16 jump() const { return this->_field<u16, 12>(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<PlayerControlsReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		return cur_size;
	}

	static void expand_raw(ss::Ref<PlayerControls> dest, ss::Expander& expander, ss::StructOffset<PlayerControlsReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		PlayerControlsReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)PlayerControls::_defaults;
		for (u32 i = 0; i < 2; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}

};

struct PlayerControlsBuilder : ss::Ref<PlayerControlsReader> {
	PlayerControlsBuilder(ss::Builder& b)
		: Ref<PlayerControlsReader>(b.alloc<PlayerControlsReader>()) {
	}

	Ref<PlayerControlsReader> done() { return std::move(*this); }

	void up(u16 v) { this->_field<u16, 0>() = v ^ 0; }
	void down(u16 v) { this->_field<u16, 2>() = v ^ 0; }
	void left(u16 v) { this->_field<u16, 4>() = v ^ 0; }
	void right(u16 v) { this->_field<u16, 6>() = v ^ 0; }
	void fire(u16 v) { this->_field<u16, 8>() = v ^ 0; }
	void change(u16 v) { this->_field<u16, 10>() = v ^ 0; }
	void jump(u16 v) { this->_field<u16, 12>() = v ^ 0; }

};

struct alignas(8) PlayerSettingsReader : ss::Struct {
	u8 data[16];

};

struct alignas(8) PlayerSettings : ss::Struct {
	static u8 _defaults[16];

	u8 data[16];

	PlayerSettings() { memcpy(data, _defaults, sizeof(_defaults)); }

	PlayerControls const& left_controls() const { return *this->_field<ss::StructOffset<PlayerControls>, 0>().get(); }
	PlayerControls const& right_controls() const { return *this->_field<ss::StructOffset<PlayerControls>, 8>().get(); }

	static usize calc_extra_size(usize cur_size, ss::Expander& expander, ss::StructOffset<PlayerSettingsReader> const& src) {
		TL_UNUSED(expander); TL_UNUSED(src);
		PlayerSettingsReader const* srcp = src.get();
		cur_size = PlayerControls::calc_extra_size(16 + cur_size, expander, srcp->_field<ss::StructOffset<PlayerControlsReader> const, 0>());
		cur_size = PlayerControls::calc_extra_size(16 + cur_size, expander, srcp->_field<ss::StructOffset<PlayerControlsReader> const, 8>());
		return cur_size;
	}

	static void expand_raw(ss::Ref<PlayerSettings> dest, ss::Expander& expander, ss::StructOffset<PlayerSettingsReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		PlayerSettingsReader const* srcp = src.get();
		u64 *p = (u64 *)dest.ptr;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)PlayerSettings::_defaults;
		for (u32 i = 0; i < 2; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto left_controls_copy = expander.alloc_uninit_raw<PlayerControls>();
		PlayerControls::expand_raw(left_controls_copy, expander, srcp->_field<ss::StructOffset<PlayerControlsReader>, 0>());
		auto right_controls_copy = expander.alloc_uninit_raw<PlayerControls>();
		PlayerControls::expand_raw(right_controls_copy, expander, srcp->_field<ss::StructOffset<PlayerControlsReader>, 8>());
		dest._field_ref<ss::StructOffset<PlayerControls>, 0>().set(left_controls_copy);
		dest._field_ref<ss::StructOffset<PlayerControls>, 8>().set(right_controls_copy);
	}

};

struct PlayerSettingsBuilder : ss::Ref<PlayerSettingsReader> {
	PlayerSettingsBuilder(ss::Builder& b)
		: Ref<PlayerSettingsReader>(b.alloc<PlayerSettingsReader>()) {
	}

	Ref<PlayerSettingsReader> done() { return std::move(*this); }

	void left_controls(ss::Ref<PlayerControlsReader> v) { return this->_field_ref<ss::StructOffset<PlayerControlsReader>, 0>().set(v); }
	void right_controls(ss::Ref<PlayerControlsReader> v) { return this->_field_ref<ss::StructOffset<PlayerControlsReader>, 8>().set(v); }

};
}

#endif
