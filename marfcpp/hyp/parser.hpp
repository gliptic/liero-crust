#ifndef PARSER_HPP
#define PARSER_HPP

#include <tl/vector.hpp>

#include "string_set.hpp"
#include "ast.hpp"

namespace hyp {

struct parser {
	u8 const* code;
	u8 const* end;
	u32 tt, token_prec;
	u32 token_data, level;
	u32 lextable[256];

	tl::vector<local> locals_buf;
	tl::vector<u8> output;
	string_set strset;

	parser(u8 const* c);

	module parse_module();
};

struct binding {
	u32 type;
};

}

#endif // PARSER_HPP
