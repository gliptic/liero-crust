#ifndef PARSER_HPP
#define PARSER_HPP

#include <tl/vector.hpp>

#include "string_set.hpp"
#include "ast.hpp"

namespace hyp {

struct parser {
	tl::vector_slice<u8> code;
	u8* cur;
	u32 tt, token_prec;
	u64 token_data;
	bool allow_comma_in_context;
	u32 level;
	u32 lextable[256];

	tl::vector<local> locals_buf;
	tl::mixed_buffer output;
	string_set strset;

	parser(tl::vector_slice<u8> c);

	module parse_module();
	bool is_err();
};

struct binding {
	u32 type;
};

}

#endif // PARSER_HPP
