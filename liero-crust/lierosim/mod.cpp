#include "mod.hpp"
#include "state.hpp"
#include <tl/wav.hpp>
#include <tl/gfx/tga.hpp>
#include <tl/approxmath/am.hpp>

namespace liero {

tl::Image read_full_tga(tl::Source file, tl::Palette* pal_out = 0) {
	tl::Image tga;
	tl::Palette pal;
	int c;
	
	{
		c = tl::read_tga(file, tga, pal);
		tl::Image tgaFull(tga.width(), tga.height(), 4);
		c = tgaFull.blit(tga.slice(), &pal);

		if (pal_out) {
			*pal_out = pal;
		}

		return std::move(tgaFull);
	}
}

Mod::Mod(tl::FsNode& root) {
	
	auto sprite_dir = root / "sprites"_S;
	auto sound_dir = root / "sounds"_S;

	this->large_sprites = read_full_tga((sprite_dir / "large.tga"_S).try_get_source(), &this->pal);
	this->small_sprites = read_full_tga((sprite_dir / "small.tga"_S).try_get_source(), &this->pal);
	this->small_font_sprites = read_full_tga((sprite_dir / "text.tga"_S).try_get_source(), &this->pal);

	worm_sprites[0] = tl::Image(16, 7 * 3 * 16, 4);
	worm_sprites[1] = tl::Image(16, 7 * 3 * 16, 4);
	muzzle_fire_sprites[0] = tl::Image(16, 7 * 16, 4);
	muzzle_fire_sprites[1] = tl::Image(16, 7 * 16, 4);

	auto worms = large_sprites.crop(tl::RectU(0, 16 * 16, 16, 16 * (16 + 7 * 3)));
	worm_sprites[0].blit(worms);
	worm_sprites[1].blit(worms, 0, 0, 0, tl::ImageSlice::Flip);

	auto muzzle_fire = large_sprites.crop(tl::RectU(0, 16 * 9, 16, 16 * (9 + 7)));
	muzzle_fire_sprites[0].blit(muzzle_fire);
	muzzle_fire_sprites[1].blit(muzzle_fire, 0, 0, 0, tl::ImageSlice::Flip);

	{
		for (u32 y = 0; y < this->large_sprites.height(); ++y) {
			LargeSpriteRow row = {0, 0};
			for (u32 x = 0; x < 16; ++x) {
				auto p = tl::Color(this->large_sprites.unsafe_pixel32(x, y));
				bool draw = p.a() > 0;
				bool half = p.a() == 1 || p.a() == 2;

				bool bit0 = draw;
				bool bit1 = !(!draw || half);
								
				row.bit0 |= bit0 << x;
				row.bit1 |= bit1 << x;
			}

			this->large_sprites_bits.push_back(row);
		}
	}

	auto r = (root / "tc.dat"_S);

	auto src = r.try_get_source();
	auto buf = src.read_all();

	ss::Expander expander(buf);

	auto* tc = expander.expand_root<TcData>(*(ss::StructOffset<TcDataReader> const*)buf.begin());

	this->tcdata = tc;
	this->weapon_types = tc->weapons().begin();
	this->level_effects = tc->level_effects().begin();
	this->nobject_types = tc->nobjects().begin();
	this->sobject_types = tc->sobjects().begin();

	for (auto& sound_name : tc->sound_names()) {
		tl::Vec<i16> sound_data;
		read_wav((sound_dir / sound_name.get()).try_get_source(), sound_data);

		this->sounds.push_back(move(sound_data));
	}

	this->mod_data = expander.to_buffer();
}

void fire(WeaponType const& self, State& state, TransientState& transient_state, Scalar angle, Vector2 vel, Vector2 pos, i16 owner) {

	//auto& mod = state.mod;

	if (owner >= 0) {
		auto& worm = state.worms.of_index(owner);
		worm.muzzle_fire = self.muzzle_fire();
	}


	
	/*
	if(w.leaveShells > 0) <--
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

	// TODO: Looping?
	transient_state.play_sound(state.mod, self.fire_sound(), transient_state);

	auto dir = sincos(angle);
	auto anglevel = vel * self.worm_vel_ratio() + dir * self.speed();

	pos += dir * i32(self.fire_offset());

	auto& ty = state.mod.get_nobject_type(self.nobject_type());

	u32 lparts = self.parts();
	for (u32 i = 0; i < lparts; ++i) {

		auto part_vel = anglevel;

		if (self.distribution() != Ratio()) {

			part_vel += rand_max_vector2(state.rand, self.distribution());
		}

		create(ty, state, angle, pos, part_vel, owner);
	}
}

}
