#include "exereader.hpp"

#include "offsets.hpp"

namespace liero {

template<typename T, usize N>
void get_array(u8 const*& b, T (&arr)[N]) {
	u8 const* p = b;
	b += sizeof(T) * N;
	memcpy(arr, p, sizeof(T) * N);
	// TODO: endianness fix
}

Ratio ratio_from_speed(u16 speed) {
	return speed / 100.0;
}

Ratio ratio(i32 num, i32 denom) {
	return Ratio(num) / denom;
}

Ratio ratio_for_bi_rand(i32 max) {
	return Ratio(max) * (1.0 / 2147483648.0);
}

#define DEF_CONSTANT_8_DESC(name, offs, conv) offs,
#define DEF_CONSTANT_16_DESC(name, offs, conv) offs,
#define DEF_CONSTANT_24_DESC(name, offs1, offs2, conv) { offs1, offs2 },
#define DEF_CONSTANT_32_DESC(name, offs1, offs2, conv) { offs1, offs2 },
#define DEF_CONSTANT_ENUM_VALUE(name, ...) name,

enum constant_8_index {
	CONSTANTS_8(DEF_CONSTANT_ENUM_VALUE)
	constant_8_count
};

enum constant_16_index {
	CONSTANTS_16(DEF_CONSTANT_ENUM_VALUE)
	constant_16_count
};

enum constant_24_index {
	CONSTANTS_24(DEF_CONSTANT_ENUM_VALUE)
	constant_24_count
};

enum constant_32_index {
	CONSTANTS_32(DEF_CONSTANT_ENUM_VALUE)
	constant_32_count
};

static u32 constant_8_offsets[constant_8_count] = {
	CONSTANTS_8(DEF_CONSTANT_8_DESC)
};

static u32 constant_16_offsets[constant_16_count] = {
	CONSTANTS_16(DEF_CONSTANT_16_DESC)
};

static u32 constant_24_offsets[constant_24_count][2] = {
	CONSTANTS_24(DEF_CONSTANT_24_DESC)
};

static u32 constant_32_offsets[constant_32_count][2] = {
	CONSTANTS_32(DEF_CONSTANT_32_DESC)
};

bool load_from_exe(ss::Builder& tcdata, tl::Source src) {
	auto window = src.window(135000);

	if (window.empty()) {
		return false;
	}

	usize const weapon_count = 40;
	usize const nobject_count = 24;
	usize const sobject_count = 14;
	usize const level_effect_count = 9;

#define DEF_WEAPON_VAR(type, name) type w_##name[weapon_count];
#define DEF_NOBJECT_VAR(type, name) type n_##name[nobject_count];
#define DEF_SOBJECT_VAR(type, name) type s_##name[sobject_count];
#define DEF_TEXTURE_VAR(type, name) type t_##name[level_effect_count];
#define DUMMY(...)

#define READ_WEAPON_VAR(type, name) get_array(b, w_##name);
#define READ_NOBJECT_VAR(type, name) get_array(b, n_##name);
#define READ_SOBJECT_VAR(type, name) get_array(b, s_##name);
#define READ_TEXTURE_VAR(type, name) get_array(b, t_##name);
#define SET_POS(pos) b = window.begin() + (pos);

#define DEF_CONSTANT_8_ACC(name, offs, conv) auto c_##name = conv(constants_8[name]);
#define DEF_CONSTANT_16_ACC(name, offs, conv) auto c_##name = conv(constants_16[name]);
#define DEF_CONSTANT_24_ACC(name, offs1, offs2, conv) auto c_##name = conv(constants_24[name]);
#define DEF_CONSTANT_32_ACC(name, offs1, offs2, conv) auto c_##name = conv(constants_32[name]);

	WEAPON_TYPES(DEF_WEAPON_VAR, DUMMY)
	NOBJECT_TYPES(DEF_NOBJECT_VAR, DUMMY)
	SOBJECT_TYPES(DEF_SOBJECT_VAR, DUMMY)
	TEXTURE_TYPES(DEF_TEXTURE_VAR, DUMMY)

	{
		u8 const* b;

		WEAPON_TYPES(READ_WEAPON_VAR, SET_POS)
		NOBJECT_TYPES(READ_NOBJECT_VAR, SET_POS)
		SOBJECT_TYPES(READ_SOBJECT_VAR, SET_POS)
		TEXTURE_TYPES(READ_TEXTURE_VAR, SET_POS)
	}

	u8 constants_8[constant_8_count];
	u16 constants_16[constant_16_count];
	i32 constants_24[constant_24_count];
	i32 constants_32[constant_32_count];

	for (usize i = 0; i < constant_8_count; ++i) {
		constants_8[i] = window[constant_8_offsets[i]];
	}

	for (usize i = 0; i < constant_16_count; ++i) {
		constants_16[i] = tl::read_le<u16>(window.begin() + constant_16_offsets[i]);
	}

	for (usize i = 0; i < constant_24_count; ++i) {
		i32 low = tl::read_le<u16>(window.begin() + constant_24_offsets[i][0]);
		i32 high = (i8)window[constant_24_offsets[i][1]];
		constants_24[i] = low + (high << 16);
	}

	for (usize i = 0; i < constant_32_count; ++i) {
		i32 low = tl::read_le<u16>(window.begin() + constant_32_offsets[i][0]);
		i32 high = tl::read_le<i16>(window.begin() + constant_32_offsets[i][1]);
		constants_32[i] = low + (high << 16);
	}

	CONSTANTS_8(DEF_CONSTANT_8_ACC);
	CONSTANTS_16(DEF_CONSTANT_16_ACC);
	CONSTANTS_24(DEF_CONSTANT_24_ACC);
	CONSTANTS_32(DEF_CONSTANT_32_ACC);

	tl::Palette pal;

	{
		u8 const* p = window.begin() + 132774;

		for (u32 i = 0; i < 256; ++i) {
			u8 r = (*p++ & 63) << 2;
			u8 g = (*p++ & 63) << 2;
			u8 b = (*p++ & 63) << 2;
			pal.entries[i] = tl::Color(r, g, b, u8(i));
		}
	}

	{
		auto root = tcdata.alloc<ss::StructOffset<TcDataReader>>();
		TcDataBuilder tc(tcdata);

		ss::ArrayBuilder<ss::StructOffset<WeaponTypeReader>> wt_arr(tcdata, weapon_count);
		ss::ArrayBuilder<ss::StructOffset<NObjectTypeReader>> nt_arr(tcdata, weapon_count + nobject_count);
		ss::ArrayBuilder<ss::StructOffset<SObjectTypeReader>> st_arr(tcdata, sobject_count);
		ss::ArrayBuilder<ss::StructOffset<LevelEffectReader>> le_arr(tcdata, level_effect_count);

		u8 const* strp = window.begin() + 0x1B676;

		for (usize i = 0; i < weapon_count; ++i) {
			WeaponTypeBuilder wt(tcdata);
			NObjectTypeBuilder nt(tcdata);

			u32 str_size = *strp++;
			if (window.end() - strp < str_size && str_size <= 13) return false;

			auto name = tcdata.alloc_str(tl::StringSlice(strp, strp + str_size));
			strp += 13;

			// Weapon
			wt.name(name);
			wt.ammo(w_ammo[i]);
			wt.delay(w_delay[i]);
			wt.distribution(ratio_for_bi_rand(w_distribution[i]));
			wt.fire_offset(w_detect_distance[i] + 5);
			wt.recoil(-ratio_from_speed(w_recoil[i])); // TODO: Handle signed recoil if hack enabled
			wt.loading_time(w_loading_time[i]);
			wt.nobject_type(u16(i));
			wt.parts(w_parts[i]);
			wt.play_reload_sound(w_play_reload_sound[i] != 0);

			Ratio worm_vel_ratio(0);
			if (w_affect_by_worm[i]) {
				w_speed[i] = tl::max(w_speed[i], i16(100));
				worm_vel_ratio = ratio(100, w_speed[i]);
			}
			wt.speed(ratio_from_speed(w_speed[i]));
			wt.worm_vel_ratio(worm_vel_ratio);

			// NObject for weapon
			nt.acceleration(ratio_from_speed(w_add_speed[i]));
			nt.blood_trail_interval(0); // wobject doesn't have blood trail
			nt.collide_with_objects(w_collide_with_objects[i] != 0);
			nt.blowaway(ratio_from_speed(w_blow_away[i]));
			nt.bounce(-ratio_from_speed(w_bounce[i]));
			nt.detect_distance(w_detect_distance[i]);
			nt.hit_damage(w_hit_damage[i]);

			NObjectAnimation anim;
			i32 num_frames;
			if (w_num_frames[i] == 0) {
				anim = NObjectAnimation::Static;
				num_frames = w_loop_anim[i] ? 1 : 0;
			} else {
				anim = w_loop_anim[i] ? NObjectAnimation::TwoWay : NObjectAnimation::OneWay;
				num_frames = w_num_frames[i];
			}

			if (w_start_frame[i] < 0 && w_color_bullets[i] > 0) {
				nt.colors()[0] = pal.entries[w_color_bullets[i] - 1];
				nt.colors()[1] = pal.entries[w_color_bullets[i]];
			}

			nt.animation(anim);
			nt.num_frames(num_frames);

			nt.drag(ratio_from_speed(w_mult_speed[i]));
			nt.expl_ground(w_expl_ground[i] != 0 && w_bounce[i] == 0); // WObjects don't explode if they are bouncy
			nt.level_effect(i16(w_dirt_effect[i]) - 1);

			nt.friction(w_bounce[i] != 100 ? ratio(4, 5) : Ratio(1)); // TODO: This should probably be read from the EXE
			nt.gravity(Scalar::from_raw(w_gravity[i]));

			nt.start_frame(w_start_frame[i]);

			nt.time_to_live(w_time_to_explo[i] ? w_time_to_explo[i] : 0xffffffff);
			nt.time_to_live_v(w_time_to_explo_v[i]);
			nt.kind((NObjectKind)w_shot_type[i]); // TODO: Validate

			auto nobj_trail_type = i32(w_part_trail_type[i]) - 1;
			if (nobj_trail_type >= 0) {
				nt.nobj_trail_interval(w_part_trail_delay[i]);
				nt.nobj_trail_scatter(ScatterType(w_part_trail_scatter[i])); // TODO: Validate
				nt.nobj_trail_type(u16(nobj_trail_type + weapon_count)); // TODO: Validate

				auto nobj_trail_speed = n_speed[nobj_trail_type];
				nt.nobj_trail_speed(ratio_from_speed(nobj_trail_speed));
				nt.nobj_trail_vel_ratio(ratio(1, 3)); // TODO: Is this something else for anything?
			}

			auto nobj_splinter_type = i32(w_splinter_type[i]) - 1;

			nt.splinter_count(w_splinter_amount[i]);
			if (nobj_splinter_type >= 0 && w_splinter_amount[i]) {
				nt.splinter_distribution(ratio_for_bi_rand(n_distribution[nobj_splinter_type]));
				nt.splinter_type(u16(nobj_splinter_type + weapon_count));
				nt.splinter_scatter(ScatterType(w_splinter_scatter[i])); // TODO: Validate
				nt.splinter_speed(ratio_from_speed(n_speed[nobj_splinter_type]));

				// TODO: Two different splinter colors
				if (w_splinter_colour[i])
					nt.splinter_color(pal.entries[w_splinter_colour[i]]);
			}

			nt.sobj_expl_type(w_create_on_exp[i] - 1); // TODO: Validate

			auto sobj_trail_type = i32(w_obj_trail_type[i]) - 1;
			if (sobj_trail_type >= 0 && w_obj_trail_delay[i]) {
				nt.sobj_trail_interval(w_obj_trail_delay[i]);
				nt.sobj_trail_type(u16(sobj_trail_type));
			}

			nt.affect_by_sobj(w_affect_by_explosions[i] != 0);
			nt.sobj_trail_when_bounced(true);
			nt.sobj_trail_when_hitting(true);
			if (w_shot_type[i] == 4) { // "laser" type
				nt.physics_speed(8);
			} else if (c_laser_weapon == i + 1) {
				nt.physics_speed(255); // TODO: Laser in Liero has infinite
			}

			wt_arr[i].set(wt.done());
			nt_arr[i].set(nt.done());
		}

		for (usize i = 0; i < nobject_count; ++i) {
			usize desti = weapon_count + i;

			//auto nt = tcdata.alloc<NObjectTypeReader>();
			NObjectTypeBuilder nt(tcdata);

			nt.blood_trail_interval(n_blood_trail[i] ? n_blood_trail_delay[i] : 0);
			nt.blowaway(ratio_from_speed(n_blow_away[i]));
			nt.bounce(-ratio_from_speed(n_bounce[i]));
			nt.friction(n_bounce[i] != 100 ? ratio(4, 5) : Ratio(1)); // TODO: This should probably be read from the EXE
			nt.collide_with_objects(false);
			nt.detect_distance(n_detect_distance[i]);
			nt.hit_damage(n_hit_damage[i]);
			nt.drag(Ratio(1));
			nt.expl_ground(n_expl_ground[i] != 0);
			nt.draw_on_level(n_draw_on_map[i] != 0);
			nt.gravity(Scalar::from_raw(n_gravity[i]));
			nt.nobj_trail_interval(0); // No trail for nobjects
			nt.sobj_expl_type(i16(n_create_on_exp[i]) - 1);
			nt.level_effect(i16(n_dirt_effect[i]) - 1);

			auto sobj_trail_type = i32(n_leave_obj[i] - 1);
			if (sobj_trail_type >= 0) {
				nt.sobj_trail_interval(n_leave_obj_delay[i]);
				nt.sobj_trail_type(u16(sobj_trail_type));
			}

			auto nobj_splinter_type = i32(n_splinter_type[i]) - 1;

			nt.splinter_count(n_splinter_amount[i]);
			if (nobj_splinter_type >= 0 && n_splinter_amount[i]) {
				nt.splinter_distribution(ratio_for_bi_rand(n_distribution[nobj_splinter_type]));
				nt.splinter_type(u16(nobj_splinter_type + weapon_count));
				nt.splinter_scatter(ScatterType::ScAngular);
				nt.splinter_speed(ratio_from_speed(n_speed[nobj_splinter_type]));

				// TODO: Two different splinter colors
				if (n_splinter_colour[i])
					nt.splinter_color(pal.entries[n_splinter_colour[i]]);
			}

			nt.animation(n_num_frames[i] ? NObjectAnimation::TwoWay : NObjectAnimation::Static);

			i32 start_frame = n_start_frame[i];

			if (n_start_frame[i] == 0) {
				nt.colors()[0] = nt.colors()[1] = pal.entries[n_color_bullets[i]];
				start_frame = -1; // Need to do this to turn on pixel rendering
			}

			nt.start_frame(start_frame);
			nt.num_frames(n_num_frames[i]);

			nt.time_to_live(n_time_to_explo[i] ? n_time_to_explo[i] : 0xffffffff);
			nt.time_to_live_v(n_time_to_explo_v[i]);

			nt.affect_by_sobj(n_affect_by_explosions[i] != 0);
			nt.sobj_trail_when_bounced(false);
			nt.sobj_trail_when_hitting(false);

			nt_arr[desti].set(nt.done());
		}

		for (usize i = 0; i < sobject_count; ++i) {
			SObjectTypeBuilder st(tcdata);

			st.anim_delay(s_anim_delay[i]);
			st.detect_range(s_detect_range[i]);
			st.level_effect(s_dirt_effect[i] - 1);
			st.num_frames(s_num_frames[i]);
			st.start_frame(s_start_frame[i]);
			// TODO: shake, flash, shadow, blow_away

			st_arr[i].set(st);
		}

		for (usize i = 0; i < level_effect_count; ++i) {
			LevelEffectBuilder le(tcdata);

			le.draw_back(!t_n_draw_back[i]);
			le.mframe(t_mframe[i]); // TODO: Validate?
			le.rframe(t_rframe[i]); // TODO: Validate?
			le.sframe(t_sframe[i]); // TODO: Validate?

			le_arr[i].set(le);
		}

		tc.nr_initial_length(ratio(c_nr_initial_length, 1 << c_nr_force_len_shl));
		tc.nr_attach_length(ratio(c_nr_attach_length, 1 << c_nr_force_len_shl));
		tc.nr_min_length(ratio(c_nr_min_length, 1 << c_nr_force_len_shl));
		tc.nr_max_length(ratio(c_nr_max_length, 1 << c_nr_force_len_shl));
		tc.nr_pull_vel(ratio(c_nr_pull_vel, 1 << c_nr_force_len_shl));
		tc.nr_release_vel(ratio(c_nr_release_vel, 1 << c_nr_force_len_shl));
		tc.nr_throw_vel(Ratio(1 << c_nr_throw_vel_x));

		tc.nr_colour_begin(c_nr_colour_begin);
		tc.nr_colour_end(c_nr_colour_end);
		tc.nr_force_mult(
			ratio((1 << c_nr_force_shl_x), c_nr_force_div_x) / (1 << c_nr_force_len_shl));
		tc.min_bounce(c_min_bounce_right);

		tc.worm_gravity(c_worm_gravity);
		tc.walk_vel(c_walk_vel_left);
		tc.max_vel(c_max_vel_right);
		tc.jump_force(c_jump_force);
		tc.max_aim_vel(c_max_aim_vel_left);
		tc.aim_acc(c_aim_acc_left);
		tc.worm_fric_mult(ratio(c_worm_fric_mult, c_worm_fric_div));

		tc.first_blood_colour(c_first_blood_colour);
		tc.num_blood_colours(c_num_blood_colours);
		tc.bobj_gravity(c_bobj_gravity);
		tc.aim_max_left(c_aim_max_left);
		tc.aim_min_left(c_aim_min_left);
		tc.aim_max_right(c_aim_max_right);
		tc.aim_min_right(c_aim_min_right);
		tc.aim_fric_mult(ratio(c_aim_fric_mult, c_aim_fric_div));

		tc.nobjects(nt_arr.done());
		tc.sobjects(st_arr.done());
		tc.weapons(wt_arr.done());
		tc.level_effects(le_arr.done());

		root.set(tc.done());
	}

	return true;
}

}
