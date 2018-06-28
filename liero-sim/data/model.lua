local memo = require "memo"
local floatconv = require "floatconv"
local Context = require "Context".Context


local function generate(header_path, source_path)

	local ctx = Context()
	local t = ctx.types

	ctx.includes[#ctx.includes + 1] = '<tl/gfx/image.hpp>'

	ctx:ExtType(t.Scalar, 'Scalar', t.I32, function(v)
		return 'Scalar::from_raw(' .. v .. ')'
	end, function(v)
		return v .. '.raw()'
	end)

	ctx:ExtType(t.Color, 'tl::Color', t.U32, function(v)
		return 'tl::Color(' .. v .. ')'
	end, function(v)
		return v .. '.v'
	end)

	local function Scalar(x)
		return math.floor(x * 65536 + 0.5)
	end

	local function RatioFromFixed(x)
		return x / 65536
	end

	local function Ratio(x, y)
		return x / (y or 1)
	end

	--ext:CheckedArrayIndex(t.NObjIdx, t.u16, )

	ctx:Enum(t.NObjectKind, t.U8,
		{'Normal', 0},
		{'DType1', 1},
		{'Steerable', 2},
		{'DType2', 3},
		{'Laser', 4})

	ctx:Enum(t.ScatterType, t.U8,
		{'ScAngular', 0},
		{'ScNormal', 1})

	ctx:Enum(t.NObjectAnimation, t.U8,
		{'TwoWay', 0},
		{'OneWay', 1},
		{'Static', 2})

	ctx:Struct(t.WeaponType,
		{'name', 0, t.String},
		{'speed', 1, t.F64},
		{'distribution', 2, t.F64},
		{'worm_vel_ratio', 3, t.F64},

		{'parts', 4, t.U32},
		{'nobject_type', 5, t.U16},
		{'loading_time', 6, t.U32},
		{'fire_offset', 7, t.U32},
		{'fire_sound', 12, t.I16},
		{'recoil', 11, t.F64},
		{'muzzle_fire', 13, t.U8},

		{'ammo', 8, t.U32},
		{'delay', 9, t.U32},
		{'play_reload_sound', 10, t.Bit})

	ctx:Struct(t.WeaponTypeList,
		{'list', 0, ctx:Array(t.WeaponType)})

	ctx:Struct(t.NObjectType,
		{'gravity', 0, t.Scalar},
		{'splinter_count', 1, t.U32},
		{'splinter_type', 2, t.U16},
		{'splinter_scatter', 3, t.ScatterType, t.ScatterType.ScNormal},
		{'splinter_distribution', 4, t.F64},
		{'splinter_speed', 46, t.F64},
		{'splinter_speed_v', 5, t.F64},
		{'splinter_color', 32, t.Color},
		
		{'time_to_live', 6, t.U32},
		{'time_to_live_v', 7, t.I32},

		{'sobj_trail_type', 8, t.U16, 0},
		{'sobj_trail_interval', 9, t.U32},
		{'sobj_trail_when_bounced', 33, t.Bit, 0},
		{'sobj_trail_when_hitting', 22, t.Bit},
		{'sobj_expl_type', 10, t.I16, -1},
		{'expl_sound', 40, t.I16, -1},

		{'bounce', 11, t.F64, Ratio(-1)},
		{'friction', 12, t.F64, Ratio(1)},
		{'drag', 13, t.F64, Ratio(1)},
		{'blowaway', 14, t.F64},
		{'acceleration', 15, t.F64},
		{'acceleration_up', 39, t.F64},
		{'affect_by_sobj', 37, t.Bit},

		{'start_frame', 16, t.I32},
		{'num_frames', 17, t.I32},
		{'colors', 18, ctx:StaticArray(t.Color, 2)},

		{'detect_distance', 19, t.U32},
		{'hit_damage', 35, t.U16},
		{'worm_coldet', 41, t.Bit},
		{'worm_col_remove_prob', 42, t.U32},
		{'worm_col_expl', 43, t.Bit},
		{'worm_col_blood', 44, t.U32},

		{'expl_ground', 20, t.Bit},
		{'draw_on_level', 38, t.Bit},
		{'level_effect', 36, t.I16, -1},
		{'animation', 34, t.NObjectAnimation, t.NObjectAnimation.TwoWay},
		{'collide_with_objects', 23, t.Bit},
		{'kind', 24, t.NObjectKind},
		
		{'blood_trail_interval', 25, t.U32, 0},
		{'nobj_trail_interval', 26, t.U32, 0},
		{'nobj_trail_type', 27, t.U16, 0},
		{'nobj_trail_scatter', 28, t.ScatterType, t.ScatterType.ScNormal},
		{'nobj_trail_vel_ratio', 29, t.F64, Ratio(1, 3)},
		{'nobj_trail_speed', 30, t.F64},
		{'nobj_trail_interval_inv', 45, t.U32, 0},
		
		{'physics_speed', 31, t.U8, 1})

		-- 46

	ctx:Struct(t.SObjectType,
		{'anim_delay', 0, t.U32},
		{'start_frame', 1, t.U16},
		{'num_frames', 2, t.U16},
		{'detect_range', 3, t.U32},
		{'level_effect', 4, t.I16, -1},
		{'start_sound', 5, t.I16, -1},
		{'worm_blow_away', 7, t.Scalar},
		{'nobj_blow_away', 8, t.Scalar},
		{'damage', 9, t.U32},
		{'num_sounds', 6, t.U8})

	ctx:Struct(t.NObjectEmitterType,
		{'speed', 0, t.F64},
		{'speed_v', 1, t.F64},
		{'distribution', 2, t.F64})

	ctx:Struct(t.LevelEffect,
		{'mframe', 0, t.U32},
		{'rframe', 1, t.U32},
		{'sframe', 2, t.U32},
		{'draw_back', 3, t.Bit})

	ctx:Struct(t.TcData,
		{'nobjects', 0, ctx:Array(t.NObjectType)},
		{'sobjects', 1, ctx:Array(t.SObjectType)},
		{'weapons', 2, ctx:Array(t.WeaponType)},
		{'level_effects', 3, ctx:Array(t.LevelEffect)},
		{'sound_names', 73, ctx:Array(t.String)},

		{'nr_initial_length', 5, t.F64, (4000 / 16)},
		{'nr_attach_length', 6, t.F64, (450 / 16)},
		{'nr_min_length', 7, t.F64, (170 / 16)},
		{'nr_max_length', 8, t.F64, (4000 / 16)},
		{'nr_throw_vel', 9, t.F64, 4},
		{'nr_force_mult', 12, t.F64},

		{'nr_pull_vel', 17, t.F64},
		{'nr_release_vel', 18, t.F64},
		{'nr_colour_begin', 19, t.U8},
		{'nr_colour_end', 20, t.U8},
		{'min_bounce', 24, t.Scalar, 53248},
		{'worm_gravity', 25, t.Scalar, 1500},
		{'walk_vel', 26, t.Scalar, 3000},
		{'max_vel', 27, t.Scalar, 29184},
		{'jump_force', 28, t.Scalar, 56064},
		{'max_aim_vel', 29, t.Scalar, 70000},
		{'aim_acc', 30, t.Scalar, 4000},
		{'aim_fric_mult', 41, t.F64, RatioFromFixed(54394)},
		{'aim_max_right', 53, t.U32, 116},
		{'aim_min_right', 54, t.U32, 64},
		{'aim_max_left', 55, t.U32, 12},
		{'aim_min_left', 56, t.U32, 64},
		{'ninjarope_gravity', 31, t.Scalar},
		{'bonus_gravity', 32, t.Scalar},
		{'worm_fric_mult', 33, t.F64, RatioFromFixed(58327)},
		{'worm_min_spawn_dist_last', 35, t.U32},
		{'worm_min_spawn_dist_enemy', 36, t.U32},
		{'bonus_bounce_mult', 42, t.F64},
		{'bonus_flicker_time', 44, t.U32},
		{'bonus_explode_risk', 45, t.Scalar},
		{'bonus_health_var', 46, t.U32},
		{'bonus_min_health', 47, t.U32},
		{'bonus_drop_chance', 52, t.F64},
		{'first_blood_colour', 58, t.U8, 80},
		{'num_blood_colours', 59, t.U8, 2},
		{'bobj_gravity', 60, t.Scalar, 1000},
		{'blood_step_up', 63, t.U32},
		{'blood_step_down', 64, t.U32},
		{'blood_limit', 65, t.U32},
		{'fall_damage_right', 66, t.U32},
		{'fall_damage_left', 67, t.U32},
		{'fall_damage_down', 68, t.U32},
		{'fall_damage_up', 69, t.U32},
		{'worm_float_level', 70, t.U32},
		{'worm_float_power', 71, t.Scalar},
		{'rem_exp_object', 72, t.I16},
		{'materials', 73, ctx:Array(t.U8, 256)},
		{'throw_sound', 74, t.U8},
		{'bonus_sobj', 75, ctx:StaticArray(t.U16, 2)},
		{'bonus_rand_timer_min', 76, ctx:StaticArray(t.U16, 2)},
		{'bonus_rand_timer_var', 77, ctx:StaticArray(t.U16, 2)},
		{'bonus_frames', 78, ctx:StaticArray(t.U16, 2)},
		{'reload_sound', 79, t.U8},
		{'blood_emitter', 80, t.NObjectEmitterType})

	ctx:Struct(t.PlayerControls,
		{'up', 0, t.U16},
		{'down', 1, t.U16},
		{'left', 2, t.U16},
		{'right', 3, t.U16},
		{'fire', 4, t.U16},
		{'change', 5, t.U16},
		{'jump', 6, t.U16})

	ctx:Struct(t.PlayerSettings,
		{'left_controls', 0, t.PlayerControls},
		{'right_controls', 1, t.PlayerControls},
		{'weapons', 2, ctx:StaticArray(t.U16, 5)})

	--[[
	ctx:Struct(t.Node,
		{'name', 0, t.String},
		ctx:Union('kind', 3, t.Bit,
			ctx:Group('file',
				{'contents', 1, ctx:Array(t.U8)},
				{'offset_in_contents', 4, t.U32}),
			{'subnodes', 2, ctx:Array(t.Node)}))

	ctx:Struct(t.Archive,
		{'tree', 0, t.Node})
	]]
	
	local namespace, header_name = 'liero', 'LIEROSIM_MODEL'

	do
		local f = io.open(header_path, "wb")
		ctx:output_hpp(namespace, header_name, function(...)
			for _,p in ipairs{...} do
				f:write(p)
			end
		end)
		f:close()
	end

	do
		local f = io.open(source_path, "wb")
		ctx:output_cpp(namespace, 'model.hpp', function(...)
			for _,p in ipairs{...} do
				f:write(p)
			end
		end)
		f:close()
	end
end

return {
	run = function(base, header_path, source_path)
		memo.run('Generate model', {base .. 'data/model.lua', base .. 'context.lua'}, {header_path, source_path}, function()
			generate(header_path, source_path)
		end)
	end
}