#ifndef OFFSETS_HPP
#define OFFSETS_HPP 1

#define CONSTANTS_32(_) \
	_(nr_initial_length, 0x32d7, 0x32dd, u32) \
	_(nr_attach_length, 0xa679, 0xa67f, u32)

#define CONSTANTS_24(_) \
	_(min_bounce_up, 0x3b7d, 0x3b74, Fixed::from_raw) \
	_(min_bounce_down, 0x3b00, 0x3af7, Fixed::from_raw) \
	_(min_bounce_left, 0x3a83, 0x3a7a, Fixed::from_raw) \
	_(min_bounce_right, 0x3a06, 0x39fd, Fixed::from_raw) \
	_(worm_gravity, 0x3bde, 0x3bd7, Fixed::from_raw) \
	_(walk_vel_left, 0x3f97, 0x3f9d, Fixed::from_raw) \
	_(max_vel_left, 0x3f8c, 0x3f83, Fixed::from_raw) \
	_(walk_vel_right, 0x4018, 0x401e, Fixed::from_raw) \
	_(max_vel_right, 0x400d, 0x4004, Fixed::from_raw) \
	_(jump_force, 0x3327, 0x332d, Fixed::from_raw) \
	_(max_aim_vel_left, 0x30f2, 0x30e9, Fixed::from_raw) \
	_(aim_acc_left, 0x30fd, 0x3103, Fixed::from_raw) \
	_(max_aim_vel_right, 0x311a, 0x3111, Fixed::from_raw) \
	_(aim_acc_right, 0x3125, 0x312b, Fixed::from_raw) \
	_(ninjarope_gravity, 0xa895, 0xa89b, Fixed::from_raw) \
	_(nr_min_length, 0x3206, 0x31fd, u32) \
	_(nr_max_length, 0x3229, 0x3220, u32) \
	_(bonus_gravity, 0x72c3, 0x72c9, Fixed::from_raw) \
	_(bobj_gravity, 0x744a, 0x7450, Fixed::from_raw) \
	/* _(worm_float_power, 0x29db, 0x29e1, Fixed::from_raw) */

#define CONSTANTS_16(_) \
	/* _(blood_limit, 0xE686, u16) */ \
	_(worm_fric_mult, 0x39BD, i16) \
	_(worm_fric_div, 0x39C7, i16) \
	/* _(worm_min_spawn_dist_last, 0x242e, i16) */ \
	/* _(worm_min_spawn_dist_enemy, 0x244b, i16) */ \
	_(aim_fric_mult, 0x3003, i16) \
	_(aim_fric_div, 0x300d, i16) \
	_(nr_throw_vel_x, 0x329b, i16) \
	/* _(nr_throw_vel_y, 0x32bf, i16) */ \
	_(nr_force_shl_x, 0xa8ad, i16) \
	_(nr_force_div_x, 0xa8b7, i16) \
	/* _(nr_force_shl_y, 0xa8da, i16) */ \
	/* _(nr_force_div_y, 0xa8e4, i16) */ \
	_(nr_force_len_shl, 0xa91e, i16) \
	/* _(bonus_bounce_mul, 0x731f, i16) */ \
	/* _(bonus_bounce_div, 0x7329, i16) */ \
	/* _(bonus_flicker_time, 0x87b8, i16) */ \
	/* _(bonus_drop_chance, 0xbeca, i16) */ \
	/* _(splinter_larpa_vel_div, 0x677d, i16) */ \
	/* _(splinter_crackler_vel_div, 0x67d0, i16) */ \
	/* _(worm_float_level, 0x29D3, i16) */

#define CONSTANTS_8(_) \
	_(aim_max_right, 0x3030, u8) \
	_(aim_min_right, 0x304a, u8) \
	_(aim_max_left, 0x3066, u8) \
	_(aim_min_left, 0x3080, u8) \
	_(nr_colour_begin, 0x10fd2, u8) \
	_(nr_colour_end, 0x11069, u8) \
	/* _(bonus_explode_risk, 0x2db2, u8) */ \
	/* _(bonus_health_var, 0x2d56, u8) */ \
	/* _(bonus_min_health, 0x2d5d, u8) */ \
	_(laser_weapon, 0x7255, u8) \
	_(first_blood_colour, 0x2388, u8) \
	_(num_blood_colours, 0x2381, u8) \
	/* _(rem_exp_object, 0x8f8b, u8) */ \
	_(nr_pull_vel, 0x31d0, i8) \
	_(nr_release_vel, 0x31f0, i8) \
/*
	_(fall_damage_right, 0x3a0e, i8) \
	_(fall_damage_left, 0x3a8b, i8) \
	_(fall_damage_down, 0x3b08, i8) \
	_(fall_damage_up, 0x3b85, i8) */ \
	/* _(blood_step_up, 0xe67b, i8) */ \
	/* _(blood_step_down, 0xe68e, i8) */

#define WEAPON_TYPES(_, pos) \
	pos(112806) \
	_(u8, detect_distance) \
	_(u8, affect_by_worm) \
	_(u8, blow_away) \
	pos(112966) \
	_(i16, gravity) \
	_(u8, shadow) \
	_(u8, laser_sight) \
	_(i8, launch_sound) \
	_(u8, loop_sound) \
	_(u8, explo_sound) \
	_(i16, speed) \
	_(i16, add_speed) \
	_(i16, distribution) \
	_(u8, parts) \
	_(u8, recoil) \
	_(u16, mult_speed) \
	_(u16, delay) \
	_(u16, loading_time) \
	_(u8, ammo) \
	_(u8, create_on_exp) \
	_(u8, dirt_effect) \
	_(u8, leave_shells) \
	_(u8, leave_shell_delay) \
	_(u8, play_reload_sound) \
	_(u8, worm_explode) \
	_(u8, expl_ground) \
	_(u8, worm_collide) \
	_(u8, fire_cone) \
	_(u8, collide_with_objects) \
	_(u8, affect_by_explosions) \
	_(u8, bounce) \
	_(u16, time_to_explo) \
	_(u16, time_to_explo_v) \
	_(u8, hit_damage) \
	_(u8, blood_on_hit) \
	_(i16, start_frame) \
	_(u8, num_frames) \
	_(u8, loop_anim) \
	_(u8, shot_type) \
	_(u8, color_bullets) \
	_(u8, splinter_amount) \
	_(u8, splinter_colour) \
	_(u8, splinter_type) \
	_(u8, splinter_scatter) \
	_(u8, obj_trail_type) \
	_(u8, obj_trail_delay) \
	_(u8, part_trail_scatter) \
	_(u8, part_trail_type) \
	_(u8, part_trail_delay)
	
#define NOBJECT_TYPES(_, pos) \
	pos(111430) \
	_(u8, detect_distance) \
	_(u16, gravity) \
	_(u16, speed) \
	_(u16, speed_v) \
	_(u16, distribution) \
	_(u8, blow_away) \
	_(u8, bounce) \
	_(u8, hit_damage) \
	_(u8, worm_explode) \
	_(u8, expl_ground) \
	_(u8, worm_destroy) \
	_(u8, blood_on_hit) \
	_(u8, start_frame) \
	_(u8, num_frames) \
	_(u8, draw_on_map) \
	_(u8, color_bullets) \
	_(u8, create_on_exp) \
	_(u8, affect_by_explosions) \
	_(u8, dirt_effect) \
	_(u8, splinter_amount) \
	_(u8, splinter_colour) \
	_(u8, splinter_type) \
	_(u8, blood_trail) \
	_(u8, blood_trail_delay) \
	_(u8, leave_obj) \
	_(u8, leave_obj_delay) \
	_(u16, time_to_explo) \
	_(u16, time_to_explo_v) \

#define SOBJECT_TYPES(_, pos) \
	pos(115218) \
	_(u8, start_sound) \
	_(u8, num_sounds) \
	_(u8, anim_delay) \
	_(u8, start_frame) \
	_(u8, num_frames) \
	_(u8, detect_range) \
	_(u8, damage) \
	_(i32, blow_away) \
	pos(115368) \
	_(u8, shadow) \
	_(u8, shake) \
	_(u8, flash) \
	_(u8, dirt_effect)

#define TEXTURE_TYPES(_, pos) \
	pos(0x1C208) \
	_(u8, n_draw_back) \
	pos(0x1C1EA) \
	_(u8, mframe) \
	pos(0x1C1F4) \
	_(u8, sframe) \
	pos(0x1C1FE) \
	_(u8, rframe)

#endif // OFFSETS_HPP
