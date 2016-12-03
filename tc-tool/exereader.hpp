#ifndef EXEREADER_HPP
#define EXEREADER_HPP 1

#include <tl/io/stream.hpp>
#include <tl/io/archive.hpp>
#include <liero-sim/data/model.hpp>

namespace liero {

bool load_from_exe(
	tl::ArchiveBuilder& archive,
	ss::Builder& tcdata,
	tl::Palette& pal,
	tl::Source src,
	u8 (&used_sounds)[256],
	tl::StreamRef sprite_stream,
	tl::TreeRef sprites_dir);

void load_from_gfx(
	tl::ArchiveBuilder& archive,
	tl::TreeRef sprites_dir,
	tl::StreamRef stream,
	tl::Palette const& pal,
	tl::Source src);

void load_from_sfx(
	tl::ArchiveBuilder& archive,
	tl::TreeRef sounds_dir,
	tl::StreamRef stream,
	tl::Source src,
	u8 (&used_sounds)[256]);

}

#endif // EXEREADER_HPP
