#include <tl/io/filesystem.hpp>
#include <tl/io/archive.hpp>
#include <tl/string.hpp>

#include "exereader.hpp"

using tl::String;

int main(int argc, char const* argv[]) {

	tl::StringSlice config_path("./"_S), exe_path, tc_name;

	for (int i = 0; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == '-' && i + 1 < argc) {
				if (strcmp(argv[i] + 2, "config-root") == 0) {
					++i;
					config_path = tl::from_c_str(argv[i]);
				} else if (strcmp(argv[i] + 2, "tc-name") == 0) {
					++i;
					tc_name = tl::from_c_str(argv[i]);
				}
			}
		} else {
			exe_path = tl::from_c_str(argv[i]);
		}
	}

	if (exe_path.empty()) {
		printf("tc-tool <path-to-tc>");
		return 0;
	}

	tl::FsNode in_node(tl::FsNode::from_path(tl::String(exe_path)));
	auto exe_node = in_node / "LIERO.EXE"_S;
	auto exe_src = exe_node.try_get_source();

	auto out_node = tl::FsNode(tl::FsNode::from_path(tl::String::concat(config_path, tc_name, ".hrc"_S)));

#if 0
	auto tcdat_node = out_node / "tc.dat"_S;

	tcdat_node
		.try_get_sink()
		.put(buf.slice_const());

	printf("Written %d bytes\n", (int)buf.size());
#else
	tl::ArchiveBuilder ar;
	auto main_str = ar.add_stream();

	tl::Palette pal;

	{
		ss::Builder tcdata;
		liero::load_from_exe(tcdata, pal, std::move(exe_src));

		auto buf = tcdata.to_vec();
		ar.add_entry_to_dir(ar.root(),
			ar.add_file(String("tc.dat"_S), main_str, buf.slice_const()));
	}

	{
		auto sprites_dir = ar.add_dir(String("sprites"_S));
		auto chr_node = in_node / "LIERO.CHR"_S;
		liero::load_from_gfx(ar, sprites_dir.contents.tree, main_str, pal, chr_node.try_get_source());
		ar.add_entry_to_dir(ar.root(), move(sprites_dir));
	}

	{
		auto sounds_dir = ar.add_dir(String("sounds"_S));
		auto snd_node = in_node / "LIERO.SND"_S;
		liero::load_from_sfx(ar, sounds_dir.contents.tree, main_str, snd_node.try_get_source());
		ar.add_entry_to_dir(ar.root(), move(sounds_dir));
	}

	auto sink = out_node.try_get_sink();

	auto vec = ar.write();
	sink.put(vec.slice_const());
	printf("Written %d bytes\n", (int)sink.position());

	auto reader = tl::ArchiveReader::from(vec.slice_const());

	/*
	auto sub = reader.root().find("tc.dat"_S);
	auto file = sub.extract();
	printf("Read sub: %d bytes\n", (int)file.size());
	*/
#endif
}
