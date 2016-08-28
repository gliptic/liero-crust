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
	
	auto sprites = root / "sprites"_S;
	auto sounds = root / "sounds"_S;

	this->large_sprites = read_full_tga((sprites / "large.tga"_S).try_get_source(), &this->pal);
	this->small_sprites = read_full_tga((sprites / "small.tga"_S).try_get_source(), &this->pal);
	this->small_font_sprites = read_full_tga((sprites / "text.tga"_S).try_get_source(), &this->pal);

	read_wav((sounds / "throw.wav"_S).try_get_source(), this->ninjarope_sound);

	worm_sprites[0] = tl::Image(16, 7 * 3 * 16, 4);
	worm_sprites[1] = tl::Image(16, 7 * 3 * 16, 4);

	auto worms = large_sprites.crop(tl::RectU(0, 16 * 16, 16, 16 * (16 + 7 * 3)));
	worm_sprites[0].blit(worms);
	worm_sprites[1].blit(worms, 0, 0, 0, tl::ImageSlice::Flip);

	auto src = (root / "tc.dat"_S).try_get_source();
	auto buf = src.read_all();

	ss::Expander expander(buf);

	auto* tc = expander.expand_root<TcData>(*(ss::StructOffset<TcDataReader> const*)buf.begin());

	this->tcdata = tc;
	this->weapon_types = tc->weapons().begin();
	this->level_effects = tc->level_effects().begin();
	this->nobject_types = tc->nobjects().begin();
	this->sobject_types = tc->sobjects().begin();
	this->mod_data = expander.to_buffer();
}

void fire(WeaponType const& self, State& state, Scalar angle, Vector2 vel, Vector2 pos) {

	//auto& mod = state.mod;

	/*
	if(w.leaveShells > 0)
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

		create(ty, state, angle, pos, part_vel);
	}
}

}
