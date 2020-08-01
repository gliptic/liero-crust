#include "exereader.hpp"

#include "offsets.hpp"
#include <liero-sim/material.hpp>
#include <tl/gfx/tga.hpp>
#include <tl/wav.hpp>
#include <tl/rand.hpp>
#include <tl/char.h>

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

Ratio ratio_for_rand(i32 max) {
	return Ratio(max) * (1.0 / 4294967296.0);
}

Ratio ratio_for_rand_ratio(Ratio max) {
	return max * (1.0 / 4294967296.0);
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

static char const* sound_names[] = {
	"shotgun.wav", "shot.wav", "rifle.wav", "bazooka.wav", "blaster.wav", "throw.wav",
	"larpa.wav", "exp3a.wav", "exp3b.wav", "exp2.wav", "exp3.wav", "exp4.wav", "exp5.wav",
	"dirt.wav", "bump.wav", "death1.wav", "death2.wav", "death3.wav", "hurt1.wav", "hurt2.wav",
	"hurt3.wav", "alive.wav", "begin.wav", "dropshel.wav", "reloaded.wav", "moveup.wav",
	"movedown.wav", "select.wav", "boing.wav", "burner.wav"
};

static tl::Image load_font_from_exe(tl::VecSlice<u8 const> src, u32 width, u32 height) {
	u32 count = 250;
	tl::Image img(width, height * count, 1);

	//assert(src.size() >= sprites_size);

	src.unsafe_cut_front(0x1C825);

	u8 const* srcp = src.begin();

	for (u32 i = 0; i < count; ++i) {

		u8 const* ptr = srcp + i*64 + 1;
		u8 char_width = tl::min(ptr[63], u8(7));

		for (u32 y = 0; y < height; ++y) {
			for (u32 x = 0; x < width; ++x) {
				u32 idx = y * 8 + x;
				img.unsafe_pixel8(x, y + i * height) = idx != 63 && ptr[idx] ? 255 : 0;
			}
		}

		img.unsafe_pixel8(char_width, i * height) = 1;
	}

	return move(img);
}

u8 expand_col(u8 v) {
	return (v << 2) | (v >> 4);
}

bool load_from_exe(
	tl::ArchiveBuilder& archive,
	ss::Builder& tcdata,
	tl::Palette& pal,
	tl::Source src,
	u8 (&used_sounds)[256],
	tl::StreamRef sprite_stream,
	tl::TreeRef sprites_dir) {

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

	{
		u8 const* p = window.begin() + 132774;

		for (u32 i = 0; i < 256; ++i) {
			u8 r = expand_col(*p++ & 63);
			u8 g = expand_col(*p++ & 63);
			u8 b = expand_col(*p++ & 63);
			pal.entries[i] = tl::Color(r, g, b, u8(i));
		}
	}

	u8 c_throw_sound = 1 + 5; // TODO: Read from EXE?
	u8 c_reload_sound = 1 + 24;

	i8 sound_index[256] = {-1};
	u32 sound_count = 0;

	{
		used_sounds[c_throw_sound] = 1;
		used_sounds[c_reload_sound] = 1;

		for (u32 i = 0; i < weapon_count; ++i) {
			used_sounds[w_launch_sound[i]] = 1;
			used_sounds[w_explo_sound[i]] = 1;
		}

		for (u32 i = 0; i < sobject_count; ++i) {
			if (s_start_sound[i]) {
				for (u8 s = 0;; ++s) {
					used_sounds[s_start_sound[i] + s] = 1;

					if (s >= s_num_sounds[i])
						break;
				}
			}
		}

		for (u32 i = 1; i < 256; ++i) {
			if (used_sounds[i]) {
				sound_index[i] = tl::narrow<i8>(sound_count++);
			}
		}
	}

	{
		auto root = tcdata.alloc<ss::StructOffset<TcDataReader>>();
		TcDataBuilder tc(tcdata);

		ss::ArrayBuilder<ss::StructOffset<WeaponTypeReader>> wt_arr(tcdata, weapon_count);
		ss::ArrayBuilder<ss::StructOffset<NObjectTypeReader>> nt_arr(tcdata, weapon_count + nobject_count);
		ss::ArrayBuilder<ss::StructOffset<SObjectTypeReader>> st_arr(tcdata, sobject_count);
		ss::ArrayBuilder<ss::StructOffset<LevelEffectReader>> le_arr(tcdata, level_effect_count);
		ss::ArrayBuilder<ss::StringOffset> snd_arr(tcdata, sound_count);
		ss::ArrayBuilder<u8> materials_arr(tcdata, 256);

		u8 const* strp = window.begin() + 0x1B676;

		for (usize i = 0; i < weapon_count; ++i) {
			WeaponTypeBuilder wt(tcdata);
			NObjectTypeBuilder nt(tcdata);

			u32 str_size = *strp++;
			// TODO: Don't understand this "&& str_size <= 13" check, shouldn't it be "|| str_size > 13", or limit str_size to 13
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
			wt.muzzle_fire(w_fire_cone[i]);
			wt.loading_time(w_loading_time[i]);
			wt.nobject_type(u16(i));
			wt.parts(w_parts[i]);
			wt.play_reload_sound(w_play_reload_sound[i] != 0);

			wt.fire_sound(sound_index[w_launch_sound[i]]);

			Ratio worm_vel_ratio(0);
			if (w_affect_by_worm[i]) {
				w_speed[i] = tl::max(w_speed[i], i16(100));
				worm_vel_ratio = ratio(100, w_speed[i]);
			}
			wt.speed(ratio_from_speed(w_speed[i]));
			wt.worm_vel_ratio(worm_vel_ratio);

			// NObject for weapon
			f64 acceleration, acceleration_up;
			f64 drag = ratio_from_speed(w_mult_speed[i]);
			if (w_shot_type[i] == 2) {
				acceleration = ratio_from_speed(w_speed[i]) / 9.0;
				acceleration_up = ratio_from_speed(w_speed[i] + w_add_speed[i]) / 9.0;
				drag = drag * 8.0 / 9.0;
			} else if (w_shot_type[i] == 3) {
				acceleration = ratio_from_speed(w_add_speed[i]);
				acceleration_up = 0.0;
			} else {
				acceleration = 0.0;
				acceleration_up = 0.0;
			}

			nt.acceleration(acceleration);
			nt.acceleration_up(acceleration_up);
			nt.drag(drag);

			nt.blood_trail_interval(0); // wobject doesn't have blood trail
			nt.collide_with_objects(w_collide_with_objects[i] != 0);
			nt.blowaway(ratio_from_speed(w_blow_away[i]));
			nt.bounce(-ratio_from_speed(w_bounce[i]));
			nt.detect_distance(w_detect_distance[i]);
			nt.hit_damage(w_hit_damage[i]);
			nt.worm_col_blood(w_blood_on_hit[i]);

			nt.worm_coldet(w_hit_damage[i] || w_blow_away[i] || w_blood_on_hit[i] || w_worm_collide[i]);
			if (w_worm_collide[i] != 0) {
				nt.worm_col_remove_prob(0xffffffff / w_worm_collide[i]);
				nt.worm_col_expl(w_worm_explode[i] != 0);
			}

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

				u32 inv = u32(0x100000000ull / w_part_trail_delay[i]);
				nt.nobj_trail_interval_inv(inv);

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
				auto speed_v = ratio_from_speed(n_speed_v[nobj_splinter_type]);
				auto speed = ratio_from_speed(n_speed[nobj_splinter_type]) - speed_v * 0.5;
				nt.splinter_speed(speed);
				nt.splinter_speed_v(ratio_for_rand_ratio(speed_v));

				// TODO: Two different splinter colors
				if (w_splinter_colour[i])
					nt.splinter_color(pal.entries[w_splinter_colour[i]]);
			}

			nt.expl_sound(sound_index[w_explo_sound[i]]);
			nt.sobj_expl_type(w_create_on_exp[i] - 1); // TODO: Validate

			auto sobj_trail_type = i32(w_obj_trail_type[i]) - 1;
			if (sobj_trail_type >= 0 && w_obj_trail_delay[i]) {
				nt.sobj_trail_interval(w_obj_trail_delay[i]);
				nt.sobj_trail_type(u16(sobj_trail_type));
			}

			nt.affect_by_sobj(w_affect_by_explosions[i] != 0);
			nt.sobj_trail_when_bounced(true);
			nt.sobj_trail_when_hitting(true);
			if (c_laser_weapon == i + 1) {
				nt.physics_speed(255); // TODO: Laser in Liero has infinite
			} else if (w_shot_type[i] == 4) { // "laser" type
				nt.physics_speed(8);
			}

			wt_arr[i].set(wt.done());
			nt_arr[i].set(nt.done());
		}

		for (usize i = 0; i < nobject_count; ++i) {
			usize desti = weapon_count + i;

			NObjectTypeBuilder nt(tcdata);

			nt.blood_trail_interval(n_blood_trail[i] ? n_blood_trail_delay[i] : 0);
			nt.blowaway(ratio_from_speed(n_blow_away[i]));
			nt.bounce(-ratio_from_speed(n_bounce[i]));
			nt.friction(n_bounce[i] != 100 ? ratio(4, 5) : Ratio(1)); // TODO: This should probably be read from the EXE
			nt.collide_with_objects(false);
			nt.drag(Ratio(1));
			nt.expl_ground(n_expl_ground[i] != 0);
			nt.draw_on_level(n_draw_on_map[i] != 0);
			nt.gravity(Scalar::from_raw(n_gravity[i]));
			nt.nobj_trail_interval(0); // No trail for nobjects
			nt.sobj_expl_type(i16(n_create_on_exp[i]) - 1);
			nt.level_effect(i16(n_dirt_effect[i]) - 1);

			nt.detect_distance(n_detect_distance[i]);
			nt.hit_damage(n_hit_damage[i]);

			nt.worm_coldet(n_hit_damage[i] != 0);
			if (n_worm_explode[i] || n_worm_destroy[i]) {
				nt.worm_col_remove_prob(0xffffffff);
				nt.worm_col_expl(n_worm_explode[i] != 0);
			}

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

				auto speed_v = ratio_from_speed(n_speed_v[nobj_splinter_type]);
				auto speed = ratio_from_speed(n_speed[nobj_splinter_type]) - speed_v * 0.5;

				nt.splinter_speed(speed);
				nt.splinter_speed_v(ratio_for_rand(speed_v));

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
			st.start_sound(sound_index[s_start_sound[i]]);
			st.num_sounds(s_num_sounds[i]);
			st.worm_blow_away(Scalar::from_raw(s_blow_away[i]));
			st.nobj_blow_away(Scalar::from_raw(s_blow_away[i]) * (1.0 / 3.0));
			st.damage(s_damage[i]);
			// TODO: shake, flash, shadow

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

		for (usize i = 0; i < sizeof(sound_names) / sizeof(*sound_names); ++i) {
			if (used_sounds[i + 1]) {
				i8 index = sound_index[i + 1];
				char const* n = sound_names[i];
				auto name = tcdata.alloc_str(tl::StringSlice((u8 const*)n, (u8 const*)n + strlen(n)));

				snd_arr[index].set(name);
			}
		}

		{
			u8 const* first = window.begin() + 0x01C2E0;
			u8 const* second = window.begin() + 0x01AEA8;

			#define BITF(bit) ((first[(bit)*32 + (i >> 3)] >> (i & 7)) & 1)
			#define BITS(bit) ((second[(bit)*32 + (i >> 3)] >> (i & 7)) & 1)

			for (usize i = 0; i < 256; ++i) {

				u8 dirt = -BITF(0);
				u8 dirt2 = -BITF(1);
				u8 rock = -BITF(2);
				u8 background = -BITF(3);
				//u8 seeshadow = -BITF(4);
				u8 wormm = -BITS(0);

				materials_arr[i].set_raw(
					(Material::Dirt & (dirt | dirt2)) |
					(Material::Rock & rock) |
					(Material::Background & background) |
					(Material::WormM & wormm));
			}
		}

		{
			auto font = load_font_from_exe(window, 8, 8);

			tl::SinkVector vecSink;
			tl::write_tga(vecSink, font.slice(), pal);

			auto vec = vecSink.unwrap_vec();

			{
				auto sink = tl::Sink::from_file("font_test.tga");
				sink.put(vec.slice_const());
			}

			auto file = archive.add_file(tl::String("font.tga"_S), sprite_stream, vec.slice_const());
			archive.add_entry_to_dir(sprites_dir, move(file));
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

		tc.bonus_bounce_mult(ratio(c_bonus_bounce_mul, c_bonus_bounce_div));
		tc.bonus_drop_chance(ratio_for_rand(c_bonus_drop_chance));
		tc.bonus_gravity(c_bonus_gravity);

		tc.throw_sound(sound_index[c_throw_sound]);
		tc.reload_sound(sound_index[c_reload_sound]);

		tc.nobjects(nt_arr.done());
		tc.sobjects(st_arr.done());
		tc.weapons(wt_arr.done());
		tc.level_effects(le_arr.done());

		tc.sound_names(snd_arr.done());
		tc.materials(materials_arr.done());

		tc.bonus_sobj()[0] = c_bonus_sobj_0 - 1;
		tc.bonus_sobj()[1] = c_bonus_sobj_1 - 1;
		tc.bonus_rand_timer_min()[0] = c_bonus_rand_timer_min_0;
		tc.bonus_rand_timer_min()[1] = c_bonus_rand_timer_min_1;
		tc.bonus_rand_timer_var()[0] = c_bonus_rand_timer_var_0;
		tc.bonus_rand_timer_var()[1] = c_bonus_rand_timer_var_1;
		tc.bonus_frames()[0] = c_bonus_frame_0;
		tc.bonus_frames()[1] = c_bonus_frame_1;

		{
			NObjectEmitterTypeBuilder et(tcdata);
			u32 blood_type = 6;
			et.distribution(ratio_for_bi_rand(n_distribution[blood_type]));
			//et.distribution(Scalar::from_raw(n_distribution[blood_type]) * 2);

			auto speed_v = ratio_from_speed(n_speed_v[blood_type]);
			auto speed = ratio_from_speed(n_speed[blood_type]) - speed_v * 0.5;
			et.speed(speed);
			et.speed_v(ratio_for_rand_ratio(speed_v));

			tc.blood_emitter(et.done());
		}
		
		root.set(tc.done());
	}

	return true;
}

static tl::Image load_sprites_from_gfx(tl::VecSlice<u8 const>& src, u32 width, u32 height, u32 count) {
	tl::Image img(width, height * count, 1);

	//auto w = src.window(width * height * count);
	usize sprites_size = width * height * count;
	assert(src.size() >= sprites_size);

	u8 const* srcp = src.begin();
	src.unsafe_cut_front(sprites_size);

	for (u32 i = 0; i < count; ++i) {
		for (u32 x = 0; x < width; ++x) {
			for (u32 y = 0; y < height; ++y) {
				img.unsafe_pixel8(x, y + i * height) = srcp[y];
			}

			srcp += height;
		}
	}

	return move(img);
}

void load_from_gfx(
	tl::ArchiveBuilder& archive,
	tl::TreeRef sprites_dir,
	tl::StreamRef stream,
	tl::Palette const& pal,
	tl::Source src) {

	auto data = src.window(35028);
	if (data.empty()) return;
	
	data.unsafe_cut_front(10);

	auto large_sprites = load_sprites_from_gfx(data, 16, 16, 110);
	data.unsafe_cut_front(4);
	auto small_sprites = load_sprites_from_gfx(data, 7, 7, 130);
	data.unsafe_cut_front(4);
	auto text_sprites = load_sprites_from_gfx(data, 4, 4, 26);
	data.unsafe_cut_front(4);

	tl::LcgPair rand(0xffff, 0xffff0000);

	for(u32 y = 0; y < 16; ++y) {
		for(u32 x = 0; x < 16; ++x) {
			large_sprites.unsafe_pixel8(x, y + 73 * 16) = u8(160 + rand.get_i32(4));
			large_sprites.unsafe_pixel8(x, y + 74 * 16) = u8(160 + rand.get_i32(4));

			large_sprites.unsafe_pixel8(x, y + 87 * 16) = u8(12 + rand.get_i32(4));
			large_sprites.unsafe_pixel8(x, y + 88 * 16) = u8(12 + rand.get_i32(4));

			large_sprites.unsafe_pixel8(x, y + 82 * 16) = u8(94 + rand.get_i32(4));
			large_sprites.unsafe_pixel8(x, y + 83 * 16) = u8(94 + rand.get_i32(4));
		}
	}

	{
		tl::SinkVector vec;
		tl::write_tga(vec, large_sprites.slice(), pal);
		auto file = archive.add_file(tl::String("large.tga"_S), stream, vec.unwrap_vec().slice_const());
		archive.add_entry_to_dir(sprites_dir, move(file));
	}

	{
		tl::SinkVector vec;
		tl::write_tga(vec, small_sprites.slice(), pal);
		auto file = archive.add_file(tl::String("small.tga"_S), stream, vec.unwrap_vec().slice_const());
		archive.add_entry_to_dir(sprites_dir, move(file));
	}

	{
		tl::SinkVector vec;
		tl::write_tga(vec, text_sprites.slice(), pal);
		auto file = archive.add_file(tl::String("text.tga"_S), stream, vec.unwrap_vec().slice_const());
		archive.add_entry_to_dir(sprites_dir, move(file));
	}
}

void load_from_sfx(
	tl::ArchiveBuilder& archive,
	tl::TreeRef sounds_dir,
	tl::StreamRef stream,
	tl::Source src,
	u8 (&used_sounds)[256]) {

	auto data = src.read_all();
	if (data.size() < 2) return;

	u32 count = tl::read_le<u16>(data.begin());
	if (data.size() < 2 + (8 + 4 + 4)*count) return;

	auto directory = data;
	directory.unsafe_cut_front(2);

	for (u32 i = 0; count; ++i) {
		
		u8 name[9];
		name[8] = 0;
		memcpy(name, directory.begin(), 8);

		usize name_len = 0;
		for (; name_len < 8 && name[name_len]; ) {
			name[name_len] = tl_char_tolower(name[name_len]);
			++name_len;
		}

		tl::StringSlice name_sl(name, name + name_len);
		directory.unsafe_cut_front(8);

		u32 offset = tl::read_le<u32>(directory.begin());
		u32 length = tl::read_le<u32>(directory.begin() + 4);
		directory.unsafe_cut_front(4 + 4);

		if (data.size() < offset + length) return;

		tl::VecSlice<u8 const> sound_data(data.begin() + offset, data.begin() + offset + length);

		if (used_sounds[i + 1]) {
			tl::SinkVector vec;
			tl::write_wav(vec, sound_data);
			auto file = archive.add_file(tl::String::concat(name_sl, ".wav"_S), stream, vec.unwrap_vec().slice_const());
			archive.add_entry_to_dir(sounds_dir, move(file));
		}
	}
}

}
