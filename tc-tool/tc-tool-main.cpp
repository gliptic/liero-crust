#include <tl/io/filesystem.hpp>
#include <tl/string.hpp>

#include "exereader.hpp"
#include "archive.hpp"

using tl::String;

int main(int argc, char const* argv[]) {

	tl::StringSlice config_path("."_S), exe_path, tc_name;

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

	tl::FsNode node((tl::String(exe_path)));
	auto exe_node = node / "LIERO.EXE"_S;
	auto src = exe_node.try_get_source();

	ss::Builder tcdata;
	liero::load_from_exe(tcdata, std::move(src));

	auto buf = tcdata.to_vec();

	tl::FsNode out_node((tl::String(config_path)));

#if 0
	auto tcdat_node = out_node / "tc.dat"_S;

	tcdat_node
		.try_get_sink()
		.put(buf.slice_const());

	printf("Written %d bytes\n", (int)buf.size());
#else
	liero::Archive ar;
	auto main_str = ar.add_stream();

	ar.add_entry_to_dir(ar.root(),
		ar.add_file(String("tc.dat"_S), main_str, buf.slice_const()));

	auto ar_node = out_node / "tc.hrc"_S;
	auto sink = ar_node.try_get_sink();

	ar.write(sink);
	printf("Written %d bytes\n", (int)sink.position());
#endif
}
