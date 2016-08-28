#include <tl/string.hpp>
#include <tl/io/stream.hpp>

namespace liero {

struct StreamRef {
	u32 index;

	StreamRef(u32 index_init) : index(index_init) {
	}
};

struct FileRef {
	FileRef(StreamRef stream, u32 offset, u32 size)
		: stream(stream), offset(offset), size(size) {
	}

	StreamRef stream;
	u32 offset;
	u32 size;
};

struct EntryRef;

struct TreeRef {
	u32 index;

	TreeRef(u32 index_init) : index(index_init) {
	}
};

union EntryContentsRef {

	EntryContentsRef(TreeRef tree_init)
		: tree(tree_init) {
	}

	EntryContentsRef(FileRef file)
		: file(file) {
	}

	~EntryContentsRef() {
	}

	TreeRef tree;
	FileRef file;
};

struct EntryRef {
	EntryRef(tl::String name, TreeRef tree)
		: name(move(name)), is_tree(true), contents(tree) {
	}

	EntryRef(tl::String name, FileRef file)
		: name(move(name)), is_tree(false), contents(file) {
	}

	tl::String name;
	EntryContentsRef contents;
	bool is_tree;
};

struct EntryTree {
	EntryTree() : pos(0) {
	}

	u32 pos;
	tl::Vec<EntryRef> children;
};

struct Stream {
	Stream(u32 encoding) : pos(0), uncompressed_size(0), encoding(encoding) {
	}

	tl::Vec<u8> data;
	u32 pos;
	u32 uncompressed_size;
	u32 encoding;
};

struct Encoding {

};

struct Archive {
	tl::Vec<EntryTree> trees;
	tl::Vec<Stream> streams;
	tl::Vec<Encoding> encoding;

	Archive() {
		trees.push_back(EntryTree());
	}

	void write(tl::Sink& out);

	TreeRef root() {
		return TreeRef(0);
	}

	StreamRef add_stream() {
		u32 index = tl::narrow<u32>(streams.size());
		streams.push_back(Stream(0));
		return StreamRef(index);
	}

	EntryRef add_file(tl::String name, StreamRef stream, tl::VecSlice<u8 const> contents) {
		auto& str = streams[stream.index];
		u32 uncompressed_size = tl::narrow<u32>(contents.size());
		u32 offset = str.uncompressed_size;
		u8* p = str.data.unsafe_alloc(contents.size());
		memcpy(p, contents.begin(), contents.size());

		str.uncompressed_size += uncompressed_size;
		return EntryRef(move(name), FileRef(stream, offset, uncompressed_size));
	}

	EntryRef add_dir(tl::String name) {
		u32 index = tl::narrow<u32>(trees.size());
		trees.push_back(EntryTree());
		return EntryRef(move(name), index);
	}

	void add_entry_to_dir(EntryRef const& parent, EntryRef child) {
		assert(parent.is_tree);
		add_entry_to_dir(parent.contents.tree, move(child));
	}

	void add_entry_to_dir(TreeRef parent, EntryRef child) {
		auto& tree = trees[parent.index];
		tree.children.push_back(move(child));
	}
};


}