#ifndef PARSER_HPP
#define PARSER_HPP

#include <tl/vec.hpp>

#include "string_set.hpp"
#include "local_set.hpp"
#include "ast.hpp"

namespace hyp {

struct parser {
	tl::VecSlice<u8> code;
	u8* cur;
	u32 tt, token_prec;
	u64 token_data;
	bool allow_comma_in_context;
	u32 level;
	u16 lextable[256];

	tl::Vec<local> locals_buf;
	local_set locals;

	tl::BufferMixed output;
	string_set strset;

	tl::BufferMixed type_output;

	//
	tl::Vec<u32> bracket_pos;
	
	parser(tl::VecSlice<u8> c);

	module parse_module();
	bool is_err();
};

struct binding {
	type_node type;
};

}

#endif // PARSER_HPP
