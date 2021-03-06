#include <tl/std.h>
#include <liero-sim/data/base_model.hpp>
#include <liero-sim/data/model.hpp>
#include <tl/io/filesystem.hpp>

/*
	FieldKind (u8):
		Array = 0
		Struct = 1
		String = 2

	FieldDesc:
		FieldKind kind
		i32 rel_offs // For Array and Struct

	u24 src_word_count
	u24 dest_word_count
	u1 has_names_and_types
	// ...
	u1  flags[src_word_count]
	u64 default[src_word_count]
	
	

	Alternative:

	Field
*/

ss::BaseRef expand_exec(ss::BaseRef dest, ss::Expander& expander, u64 const* program, ss::Offset const& src) {
	//u64 *p = (u64 *)dest.ptr;
	u8 const *s = src.ptr();
	u32 src_size = src.size;

	{
		u64 op = *program++;
		u32 src_word_count = op & 0xffffff;
		u32 dest_word_count = (op >> 24) & 0xffffff;

		u64 const *flags = program;
		u64 const *default_data = program + (src_word_count + 63) / 64;

		auto destp = dest;

		for (u32 i = 0; i < src_word_count; ++i) {
			u8 expanded = (flags[i >> 6] >> (i & 63)) & 1;
			u64 info = *default_data++;
			if (expanded) {
				// Expanding struct in-place

				i32 rel_offs = i32(info & 0xffffffff);

				switch ((info >> 32) & 0xff) {
				case 0: { // Inline struct
					u64 const* subprogram = program + rel_offs; // TODO: Overflow in multiply?
					if (i < src_size) {
						auto const& offs = ((ss::Offset const *)s)[i];
						destp = expand_exec(destp, expander, subprogram, offs);
					} else {
						ss::Offset dummy_offset;
						dummy_offset.set(u32(0), 0);
						destp = expand_exec(destp, expander, subprogram, dummy_offset);
					}
					break;
				}

				case 1: { // Array
					if (i < src_size) {
						auto const& offs = ((ss::Offset const *)s)[i];

						u64 const* subprogram = program + rel_offs;
						u64 subheader = *subprogram;
						u32 sub_dest_word_count = (subheader >> 24) & 0xffffff;

						usize array_size = sub_dest_word_count * offs.size; // TODO: Overflow check
						auto arr = expander.alloc_uninit_raw<u64>(array_size);

						ss::BaseRef p = arr;
						ss::Offset *sub_src = (ss::Offset *)offs.ptr();
						for (u32 i = 0; i < offs.size; ++i) {
							p = expand_exec(p, expander, subprogram, *sub_src);
							++sub_src;
						}

						((ss::Offset *)dest.ptr)->set(arr.ptr, offs.size);

					} else {
						// Empty array
						((ss::Offset *)dest.ptr)->set(u32(0), 0);
					}
					break;
				}

				case 2: { // Plain array
					if (i < src_size) {
						auto const& offs = ((ss::Offset const *)s)[i];

						u32 sub_dest_byte_count = info & 0xffffffff;

						usize array_size_bytes = sub_dest_byte_count * offs.size; // TODO: Overflow check
						auto arr = expander.alloc_uninit_raw<u8>(array_size_bytes);

						u8 const *sub_src = offs.ptr();
						memcpy(arr.ptr, sub_src, array_size_bytes);
						((ss::Offset *)dest.ptr)->set(arr.ptr, offs.size);

					} else {
						// Empty array
						((ss::Offset *)dest.ptr)->set(u32(0), 0);
					}
					break;
				}

				}
			} else {
				*destp.ptr = info ^ (i < src_size ? ((u64 const *)s)[i] : 0);
				destp = destp.add_words(1);
			}
		}

		return destp;
	}
}

/*
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
}*/

struct LabelRef {
	u32 label;
	u32 pos;
	i32 struct_offset;

	LabelRef() {
	}

	LabelRef(u32 label, u32 pos, u32 struct_offset)
		: label(label), pos(pos), struct_offset(struct_offset) {
	}
};

void test_ser2() {
	auto root = tl::FsNode(tl::FsNode::from_path("TC/lierov133"_S));

	auto r = (root / "tc.dat"_S);

	auto src = r.try_get_source();
	auto buf = src.read_all();

	ss::Expander expander(buf);

	auto tc = (liero::TcData *)expander.expand(liero::TcData::expand_program(), buf.begin());

	//auto tc = (liero::TcData *)expander.word(dest);

	u8 const* str = tc->weapons()[0].name().begin();
	str = tc->sound_names()[0].get().begin();

	u32 x = tc->weapons()[0].ammo();
	printf("");


	/*
	auto* tc = expander.expand_root<TcData>(*(ss::StructOffset<TcDataReader> const*)buf.begin());

	this->tcdata = tc;
	this->weapon_types = tc->weapons().begin();
	this->level_effects = tc->level_effects().begin();
	this->nobject_types = tc->nobjects().begin();
	this->sobject_types = tc->sobjects().begin();
	*/
}

void test_ser() {
	u64 info_buf[1024];
	u64 *info_write = info_buf;
	u64 *flags_cur = 0;
	u64 *start_struct = 0;
	u32 flags_idx = 0;
	//u32 labels[1024] = {};
	//u32 label_idx = 0;
	tl::Vec<u32> labels;
	//LabelRef label_refs[1024];
	//u32 label_ref_idx = 0;
	tl::Vec<LabelRef> label_refs;

	auto w = [&](u64 v) { *info_write++ = v; };
	auto l = [&]() { auto i = tl::narrow<u32>(labels.size()); labels.push_back(0); return i; };
	auto defl = [&](u32 label) { labels[label] = info_write - info_buf; };

	auto usel = [&](u32 label) {
		u32 pos = info_write - info_buf;
		//label_refs[label_ref_idx++] = LabelRef(label, pos, start_struct + 1 - info_buf);
		label_refs.push_back(LabelRef(label, pos, start_struct + 1 - info_buf));
	};

	auto finish_labels = [&]() {
		//for (u32 i = 0; i < label_refs.size(); ++i) {
		for (auto const& lr : label_refs) {
			u32 pos = lr.pos;
			info_buf[pos] = (info_buf[pos] | u32((i32)labels[lr.label] - lr.struct_offset));
		}
	};

	auto start_fields = [&](u32 src_word_count) {
		flags_idx = 0;
		flags_cur = info_write;
		auto flag_words = (src_word_count + 63) / 64;
		memset(flags_cur, 0, flag_words * 8);
		info_write += flag_words;
	};

	auto header = [&](u32 src_word_count, u32 dest_word_count) {
		start_struct = info_write;
		w(src_word_count | (dest_word_count << 24));
		start_fields(src_word_count);
	};

	auto prim_word = [&](u64 def) {
		if (flags_idx++ == 64) {
			flags_idx = 0;
			++flags_cur;
		}

		*info_write++ = def;
	};

	auto arr_word = [&](u32 struct_label) {
		*flags_cur |= u64(1) << flags_idx;
		if (flags_idx++ == 64) {
			flags_idx = 0;
			++flags_cur;
		}

		usel(struct_label);
		*info_write++ = 0 | (u64(1) << 32);
	};

	auto main = l();
	auto sub = l();
	defl(main);
		header(1, 1);
		arr_word(sub);

	defl(sub);
		header(2, 2);
		prim_word(0x1234);
		prim_word(0x5678);

	finish_labels();

	ss::Builder builder;
	auto b = builder.alloc_words(1);
	
	auto arr = builder.alloc_words(1);

	auto r = builder.alloc_words(2);
	r._field<u64, 0>() = 0;
	r._field<u64, 8>() = 0;

	arr._field<ss::Offset, 0>().set(r.ptr, 2);

	b._field<ss::Offset, 0>().set(arr.ptr, 1);

	auto vec = builder.to_vec();

	ss::Expander expander(vec.slice_const());

#if 0
	expand_exec(r, expander, info_buf + labels[main], *(ss::Offset const *)vec.begin());
#else
	auto program = info_buf + labels[main];
	u64 op = *program;
	u32 dest_word_count = (op >> 24) & 0xffffff;
	auto dest = expander.alloc_uninit<u64>(dest_word_count);

	expander.expand_exec_alloc(dest, program, *(ss::Offset const *)vec.begin());
#endif

}

void test_ser3() {

}