#ifndef LIEROSIM_MODEL_HPP
#define LIEROSIM_MODEL_HPP 1

#include <tl/std.h>
#include <tl/bits.h>
#include "base_model.hpp"

#include <tl/image.hpp>

namespace liero {

struct WeaponType;
struct WeaponTypeList;
struct NObjectType;
struct SObjectType;
struct TcData;

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

struct alignas(8) WeaponTypeBase {
	static u8 _defaults[48];

	ss::StringRef name;
	Scalar speed;
	Scalar distribution;
	Scalar worm_vel_ratio;
	u32 parts;
	u16 nobject_type;
	u8 _bits208;
	u8 _padding216;
	u32 loading_time;
	u32 fire_offset;
	u32 ammo;
	u32 delay;
	u32 _padding352;

	tl::StringSlice name_unchecked() const {
		return this->name.get();
	}

	void name_unchecked(tl::StringSlice str) {
		this->name.set(str);
	}

	bool play_reload_sound() const {
		return bool((this->_bits208 >> 0) & 1);
	}

	void play_reload_sound(bool v) {
		this->_bits208 = (this->_bits208 & 0xfe) | (v << 0);
	}
};

static_assert(sizeof(WeaponTypeBase) == 48, "Unexpected size");

struct WeaponTypeReader : WeaponTypeBase {
};

struct WeaponType : WeaponTypeBase {

	static void expand(ss::WriterRef<WeaponType> dest, ss::Expander& expander, ss::StructRef<WeaponTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		WeaponTypeReader const* srcp = src.get();
		u64 *p = (u64 *)&*dest;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)_defaults;
		for (u32 i = 0; i < 6; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto name_copy = expander.alloc_str(srcp->name.get());
		auto* destp = &*dest;
		destp->name_unchecked(*name_copy);
	}
};

struct alignas(8) WeaponTypeListBase {
	static u8 _defaults[8];

	ss::Offset list;
};

static_assert(sizeof(WeaponTypeListBase) == 8, "Unexpected size");

struct WeaponTypeListReader : WeaponTypeListBase {

	ss::ArrayRef<ss::StructRef<WeaponTypeReader>>& list_unchecked() {
		return *(ss::ArrayRef<ss::StructRef<WeaponTypeReader>>*)&this->list;
	}
};

struct WeaponTypeList : WeaponTypeListBase {

	ss::ArrayRef<WeaponType>& list_unchecked() {
		return *(ss::ArrayRef<WeaponType>*)&this->list;
	}

	static void expand(ss::WriterRef<WeaponTypeList> dest, ss::Expander& expander, ss::StructRef<WeaponTypeListReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		WeaponTypeListReader const* srcp = src.get();
		u64 *p = (u64 *)&*dest;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)_defaults;
		for (u32 i = 0; i < 1; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto list_copy = expander.expand_array<WeaponType, ss::StructRef<WeaponTypeReader>>(srcp->list);
		auto* destp = &*dest;
		destp->list.ptr(&*list_copy);
	}
};

struct alignas(8) NObjectTypeBase {
	static u8 _defaults[96];

	Scalar gravity;
	u32 splinter_count;
	u16 splinter_type;
	ScatterType splinter_scatter;
	u8 _bits88;
	Scalar splinter_distribution;
	Scalar splinter_speed;
	u32 time_to_live;
	u32 time_to_live_v;
	u16 sobj_trail_type;
	i16 sobj_expl_type;
	u32 sobj_trail_interval;
	Scalar bounce;
	Scalar friction;
	Scalar drag;
	Scalar blowaway;
	Scalar acceleration;
	i32 start_frame;
	i32 num_frames;
	tl::Color colors[2];
	u32 detect_distance;
	NObjectKind type;
	ScatterType nobj_trail_scatter;
	u16 nobj_trail_type;
	u32 blood_trail_interval;
	u32 nobj_trail_interval;
	Scalar nobj_trail_vel_ratio;
	Scalar nobj_trail_speed;

	bool expl_ground() const {
		return bool((this->_bits88 >> 0) & 1);
	}

	void expl_ground(bool v) {
		this->_bits88 = (this->_bits88 & 0xfe) | (v << 0);
	}

	bool directional_animation() const {
		return bool((this->_bits88 >> 1) & 1);
	}

	void directional_animation(bool v) {
		this->_bits88 = (this->_bits88 & 0xfd) | (v << 1);
	}

	bool sobj_trail_when_hitting() const {
		return bool((this->_bits88 >> 2) & 1);
	}

	void sobj_trail_when_hitting(bool v) {
		this->_bits88 = (this->_bits88 & 0xfb) | (v << 2);
	}

	bool collide_with_objects() const {
		return bool((this->_bits88 >> 3) & 1);
	}

	void collide_with_objects(bool v) {
		this->_bits88 = (this->_bits88 & 0xf7) | (v << 3);
	}

	bool sobj_trail_when_bounced() const {
		return bool((this->_bits88 >> 4) & 1);
	}

	void sobj_trail_when_bounced(bool v) {
		this->_bits88 = (this->_bits88 & 0xef) | (v << 4);
	}
};

static_assert(sizeof(NObjectTypeBase) == 96, "Unexpected size");

struct NObjectTypeReader : NObjectTypeBase {
};

struct NObjectType : NObjectTypeBase {

	static void expand(ss::WriterRef<NObjectType> dest, ss::Expander& expander, ss::StructRef<NObjectTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		NObjectTypeReader const* srcp = src.get();
		u64 *p = (u64 *)&*dest;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)_defaults;
		for (u32 i = 0; i < 12; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}
};

struct alignas(8) SObjectTypeBase {
	static u8 _defaults[16];

	u32 anim_delay;
	u16 start_frame;
	u16 num_frames;
	u32 detect_range;
	u16 level_effect;
	u16 _padding112;
};

static_assert(sizeof(SObjectTypeBase) == 16, "Unexpected size");

struct SObjectTypeReader : SObjectTypeBase {
};

struct SObjectType : SObjectTypeBase {

	static void expand(ss::WriterRef<SObjectType> dest, ss::Expander& expander, ss::StructRef<SObjectTypeReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		SObjectTypeReader const* srcp = src.get();
		u64 *p = (u64 *)&*dest;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)_defaults;
		for (u32 i = 0; i < 2; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
	}
};

struct alignas(8) TcDataBase {
	static u8 _defaults[32];

	ss::Offset nobjects;
	ss::Offset sobjects;
	ss::Offset weapons;
	Scalar min_bounce_up;
	u32 _padding224;
};

static_assert(sizeof(TcDataBase) == 32, "Unexpected size");

struct TcDataReader : TcDataBase {

	ss::ArrayRef<ss::StructRef<NObjectTypeReader>>& nobjects_unchecked() {
		return *(ss::ArrayRef<ss::StructRef<NObjectTypeReader>>*)&this->nobjects;
	}

	ss::ArrayRef<ss::StructRef<SObjectTypeReader>>& sobjects_unchecked() {
		return *(ss::ArrayRef<ss::StructRef<SObjectTypeReader>>*)&this->sobjects;
	}

	ss::ArrayRef<ss::StructRef<WeaponTypeReader>>& weapons_unchecked() {
		return *(ss::ArrayRef<ss::StructRef<WeaponTypeReader>>*)&this->weapons;
	}
};

struct TcData : TcDataBase {

	ss::ArrayRef<NObjectType>& nobjects_unchecked() {
		return *(ss::ArrayRef<NObjectType>*)&this->nobjects;
	}

	ss::ArrayRef<SObjectType>& sobjects_unchecked() {
		return *(ss::ArrayRef<SObjectType>*)&this->sobjects;
	}

	ss::ArrayRef<WeaponType>& weapons_unchecked() {
		return *(ss::ArrayRef<WeaponType>*)&this->weapons;
	}

	static void expand(ss::WriterRef<TcData> dest, ss::Expander& expander, ss::StructRef<TcDataReader> const& src) {
		TL_UNUSED(expander);
		u32 src_size = src.size;
		TcDataReader const* srcp = src.get();
		u64 *p = (u64 *)&*dest;
		u64 const *s = (u64 const*)srcp;
		u64 const *d = (u64 const*)_defaults;
		for (u32 i = 0; i < 4; ++i) {
			p[i] = d[i] ^ (i < src_size ? s[i] : 0);
		}
		auto nobjects_copy = expander.expand_array<NObjectType, ss::StructRef<NObjectTypeReader>>(srcp->nobjects);
		auto sobjects_copy = expander.expand_array<SObjectType, ss::StructRef<SObjectTypeReader>>(srcp->sobjects);
		auto weapons_copy = expander.expand_array<WeaponType, ss::StructRef<WeaponTypeReader>>(srcp->weapons);
		auto* destp = &*dest;
		destp->nobjects.ptr(&*nobjects_copy);
		destp->sobjects.ptr(&*sobjects_copy);
		destp->weapons.ptr(&*weapons_copy);
	}
};
}

#endif
