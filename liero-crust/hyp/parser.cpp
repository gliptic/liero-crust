#include "ast.hpp"
#include "parser.hpp"
#include <utility>

#include <tl/region.h>

using std::move;

namespace hyp {

enum {
	// digit
	// beg ident
	// beg ident + inner ident
	// inner ident
	// op [0..4]

	LEX_DIGIT = 1,
	LEX_BEGIDENT = 2,
	LEX_INNERIDENT = 4,
	LEX_WHITESPACE = 8,
	LEX_OP = 16,
	LEX_SINGLECHAR = 32,
	LEX_NEWLINE = 64
	// = 128
};

enum : u32 {
	TT_LBRACE = 1,
	TT_RBRACE = 2,
	TT_LPAREN = 3,
	TT_RPAREN = 4,
	TT_LBRACKET = 5,
	TT_RBRACKET = 6,
	TT_COLON = 7,
	TT_SEMICOLON = 8,
	TT_COMMA = 9,
	TT_BAR = 10,
	TT_LITERALINT = 11,
	TT_OP = 12,
	TT_IDENT = 13,
	TT_EQ = 14,
	TT_LET = 15,
	TT_FN = 16,
	TT_EOF = 17,
	TT_ERR = 18
};

#define CH (*cur)
#define NEXT (++cur)

static node rlambda(parser* self, u32 end_token, bool short_syntax = false);
static node rexpr(parser* self);

int const lexshift = 8;

inline u32 narrow(usize v) {
	assert(v < (1ull << 32));
	return (u32)v;
}

typedef tl::VecSmall<node> small_node_array;
typedef tl::VecSmall<binding> small_binding_array;
typedef tl::VecSmall<local> small_local_array;

template<bool Skip = false>
static int next(parser* self, bool allow_comma = true) {
	u8 ch;
	u8* cur = self->cur;

	TL_UNUSED(allow_comma); // May be unused when Skip
	assert(self->tt != TT_ERR);

	ch = CH;
	u16 lexcat = self->lextable[ch];
	NEXT;

other:
	if (lexcat & (LEX_OP | LEX_BEGIDENT)) {

		u8* ident_beg = cur - 1;
		u32 cont_mask = (lexcat & LEX_OP) ? LEX_OP : LEX_INNERIDENT;

		u64 keyw = ch | 0x100;
		u32 h = (u32)keyw;

		while (self->lextable[ch = CH] & cont_mask) {
			if (!Skip) {
				keyw = (keyw << 8) + ch;
				//h = (h * 33) ^ keyw;
				h = ((h << 17) | (h >> (32-17))) + (u32)keyw;
			}
			NEXT;
		}

		if (!Skip) {
			u32 len = (u32)(cur - ident_beg);
			if (len <= 7) {
				self->token_data = strref::from_raw(keyw).raw();
			} else {
				h *= 2 * h + 1;
				if (!h) h = 1;

				self->strset.source = tl::VecSlice<u8>(self->code.begin(), ident_beg);

				u32 ref;
				self->strset.get(h, ident_beg, len, &ref);
				self->token_data = strref::from_offset(ref, len).raw();
			}

			if (self->token_data == str_short1('|').raw()) {
				self->tt = TT_BAR;
			} else if (self->token_data == str_short1('=').raw()) {
				self->tt = TT_EQ;
			} else if (self->token_data == str_short3('l','e','t').raw()) {
				self->tt = TT_LET;
			} else if (self->token_data == str_short2('f', 'n').raw()) {
				self->tt = TT_FN;
			} else {
				self->tt = (lexcat & LEX_OP) ? TT_OP : TT_IDENT;
				self->token_prec = lexcat >> lexshift;
			}
		}
	} else if (lexcat & LEX_SINGLECHAR) {
		if (!Skip) {
			self->tt = lexcat >> lexshift;
		} else {
			self->cur = cur;
			return lexcat >> lexshift;
		}
	} else if (lexcat & LEX_WHITESPACE) {
		do {
#pragma warning(disable: 4127)
			if (!Skip && (lexcat & LEX_NEWLINE) && allow_comma && self->allow_comma_in_context) {
#pragma warning(default: 4127)
				self->tt = TT_COMMA;
				goto done;
			}
			ch = CH;
			lexcat = self->lextable[ch];
			NEXT;
		} while (lexcat & LEX_WHITESPACE);

		goto other;
		
	} else { // if (lexcat & LEX_DIGIT) {
#if 1
		char const* start = (char const*)cur - 1;

		u32 num = ch - '0';
		while (self->lextable[ch = CH] & LEX_DIGIT) {
			if (!Skip) num = (num * 10) + (ch - '0');
			NEXT;
		}

#if 0
		if (self->ch == '.' || self->ch == 'e' || self->ch == 'E') {
			tl_value v;
			tl_strscan_number(start, (char const*)code - start, &v);
			num = v.i;
		}
#else
		TL_UNUSED(start);
#endif
#else
		char const* start = (char const*)code - 1;
		while (self->lextable[self->ch] == LEX_DIGIT) {
			Parser_nextch(self);
		}

		tl_value v;

		tl_strscan_number(start, (char const*)code - start, &v);

		u32 num = v.i;
#endif
		if (!Skip) {
			self->tt = TT_LITERALINT;
			self->token_data = num;
		}
	}

done:
	//printf("T: %d\n", self->tt);

	self->cur = cur;
	return 0;
}


parser::parser(tl::VecSlice<u8> c)
	: level(0), code(c), tt(TT_COMMA)
	, allow_comma_in_context(false)
{

	for (u8 i = 0; ++i != 0;) this->lextable[i] = LEX_SINGLECHAR | (TT_ERR << lexshift);

	this->lextable[0] = LEX_SINGLECHAR | (TT_EOF << lexshift);

	for (u8 i = 'a'; i <= 'z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 'A'; i <= 'Z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 128; i != 0; ++i) this->lextable[i] = LEX_INNERIDENT;
	for (u8 i = '0'; i <= '9'; ++i) this->lextable[i] = LEX_INNERIDENT | LEX_DIGIT;

	this->lextable['\''] = LEX_BEGIDENT;

	this->lextable['.'] = LEX_BEGIDENT;
	this->lextable[' '] = LEX_WHITESPACE;
	this->lextable['\r'] = LEX_WHITESPACE | LEX_NEWLINE;
	this->lextable['\n'] = LEX_WHITESPACE | LEX_NEWLINE;

	this->lextable['{'] = LEX_SINGLECHAR + (TT_LBRACE << lexshift);
	this->lextable['}'] = LEX_SINGLECHAR + (TT_RBRACE << lexshift);
	this->lextable['('] = LEX_SINGLECHAR + (TT_LPAREN << lexshift);
	this->lextable[')'] = LEX_SINGLECHAR + (TT_RPAREN << lexshift);
	this->lextable['['] = LEX_SINGLECHAR + (TT_LBRACKET << lexshift);
	this->lextable[']'] = LEX_SINGLECHAR + (TT_RBRACKET << lexshift);
	this->lextable[':'] = LEX_SINGLECHAR + (TT_COLON << lexshift);
	this->lextable[';'] = LEX_SINGLECHAR + (TT_SEMICOLON << lexshift);
	this->lextable[','] = LEX_SINGLECHAR + (TT_COMMA << lexshift);

	this->lextable['='] = LEX_OP + (0 << lexshift);
	this->lextable['|'] = LEX_OP + (0 << lexshift);
	this->lextable['&'] = LEX_OP + (1 << lexshift);
	this->lextable['<'] = LEX_OP + (2 << lexshift);
	this->lextable['>'] = LEX_OP + (2 << lexshift);
	this->lextable['+'] = LEX_OP + (3 << lexshift);
	this->lextable['-'] = LEX_OP + (3 << lexshift);
	this->lextable['*'] = LEX_OP + (4 << lexshift);
	this->lextable['/'] = LEX_OP + (4 << lexshift);
	this->lextable['\\'] = LEX_OP + (4 << lexshift);

	u8* start = code.begin();

#if 0
	this->cur = start;
	TL_TIME(brackets, {
		while (true) {
			u8* c = this->cur;
			int tt = next<true>(this);

			if (tt == TT_LBRACE || tt == TT_RBRACE) {
				bracket_pos.push_back(c - start);
			} else if (tt == TT_EOF) {
				break;
			}
		}
	});

	printf("Bracket time: %f seconds\n", (double)brackets / 10000000.0);
#endif

	this->cur = start;
	next(this, false);
}

bool parser::is_err() {
	return this->tt == TT_ERR;
}

#undef NEXT
#undef CH

#define HOP_PARSE 1

#define ERR() { self->tt = TT_ERR; return err_ret; }
#define PEEKCHECK(t) if (self->tt != (t)) { ERR(); } else (void)0

#define CHECK(t) { PEEKCHECK(t); next(self); }
#define CHECK_NOTERR() if (self->tt == TT_ERR) { return err_ret; } else next(self)
#define TEST(t) (self->tt == (t) && (next(self), true))

#define CHECK_NC(t) { PEEKCHECK(t); next(self, false); }
#define CHECK_NOTERR_NC() if (self->tt == TT_ERR) { return err_ret; } else next(self, false)
#define TEST_NC(t) (self->tt == (t) && (next(self, false), true))

#define REF (narrow(self->output.size()) + 0)
#define DEREF(r) (self->output.begin() + (r))

// TODO: Internalize bigger constants

static node kint32(parser* self, i32 v) {
	TL_UNUSED(self);
	return node::make(NT_KI32, 0, v);
}

#  define SCOPE_BEGIN() \
	u32 parsed_nodes = 0; \
	small_local_array local_buf; \
	++self->level;

#  define SCOPE_END() { \
	--self->level; \
	for (local *b = local_buf.begin(), *e = local_buf.end(); b != e; ++b) { \
		self->locals.remove(b->name.raw()); \
	} \
}

#define ADD_LOCAL(loc) { add_local(self, (loc)); local_buf.push_back(loc); }
#define FIND_LOCAL(name) find_local(self, (name));

struct off_vec {
	u32 len;
	u32 offsets[];
};

#define NT_EXT_REFARR0 NT_CALL
#define NT_EXT_REFARR1 NT_LAMBDA
#define NT_EXT_REFARRN NT_NAME

static void add_local(parser* self, local loc) {
	//self->locals_buf.push_back(loc);
	u64 raw_name = loc.name.raw();
	u64* v = self->locals.get(raw_name);

	*v = loc.ref.get();
}


static node find_local(parser* self, strref name) {
	u64 raw_name = name.raw();
	u64* v = self->locals.get(raw_name);
	u64 vv = *v;
	u32 type = vv & 0xf;

	if (type == NT_EXT_REFARR0) {
		return node::make_str(name);
	} else {
		return node(vv);
	}
}

static node rname(parser* self) {
	strref name = strref::from_raw(self->token_data);
	next(self);
	return FIND_LOCAL(name);
}

static node rtype(parser* self) {
	switch (self->tt) {
		case TT_IDENT: {
			node local = rname(self);

			return local;
		}

		default: {
			return node_none;
		}
	}
}

static void rrecord_body(parser* self, small_node_array& expr_arr) {

	bool old_allow_comma = self->allow_comma_in_context;
	self->allow_comma_in_context = true;

	for (;;) {

		if (self->tt == TT_RBRACE || self->tt == TT_RPAREN || self->tt == TT_RBRACKET || self->tt == TT_EOF) {
			break;
		}

		node v = rexpr(self);
		expr_arr.push_back(v);

		if (!TEST_NC(TT_COMMA))
			break;
	}

	self->allow_comma_in_context = old_allow_comma;
}

#define FLUSH_PARSES() { \
	if (parsed_nodes != (u32)expr_arr.size()) { \
		flush_parses(self, expr_arr.begin() + parsed_nodes, expr_arr.end()); \
		parsed_nodes = (u32)expr_arr.size(); \
	} \
}

static int flush_parses(parser* self, node* b, node* e) {
	int err_ret = 1;

	auto old_cur = self->cur;
	auto old_tt = self->tt;
	auto old_token_data = self->token_data;
	auto old_token_prec = self->token_prec;

	for (; b != e; ++b) {
		//self->tt = TT_LBRACE;
		self->cur = self->code.begin() + b->get();

		next(self, false);
		auto l = rlambda(self, TT_RBRACE, true);
		CHECK_NOTERR();
		*b = l;
	}

	self->cur = old_cur;
	self->tt = old_tt;
	self->token_data = old_token_data;
	self->token_prec = old_token_prec;
	return 0;
}

static void skip(parser* self) {
	int nesting = 0;

	while (true) {
		auto t = next<true>(self);

		if (t == TT_LBRACE) {
			++nesting;
		} else if (t == TT_RBRACE) {
			--nesting;

			if (nesting == 0) {
				next(self);
				break;
			}

		} else if (t == TT_EOF) {
			self->tt = TT_ERR;
			return;
		}
	}
}

template<typename T>
inline usize copy_to(T& self, void* dest) {
	usize len = self.size_bytes();
	memcpy(dest, self.begin(), len);
	return len;
}

inline void copy_params(parser* self, small_node_array& params, node* dest) {
	TL_UNUSED(self);
	memcpy(dest, params.begin(), params.size_bytes());
}

/*
TODO: Parse function signature into type. Store type/source position into parent for later parsing of body.
*/

static node rlambda(parser* self, u32 end_token, bool parse_arguments) {
	node err_ret(0);

	small_binding_array binding_arr;
	small_node_array expr_arr;

	{
		SCOPE_BEGIN();

		bool old_allow_comma = self->allow_comma_in_context;
		self->allow_comma_in_context = true;

		if (TEST_NC(TT_BAR)) {
			if (self->tt != TT_BAR) {
				do {
					strref name = strref::from_raw(self->token_data);
					CHECK(TT_IDENT);

					node upv = node::make_upval(self->level, narrow(local_buf.size()));
					local loc = { name, upv };
					ADD_LOCAL(loc);

					if (TEST_NC(TT_COLON)) {
						rtype(self);
					}

				} while (TEST_NC(TT_COMMA));
			}

			CHECK_NC(TT_BAR);
		}

		if (parse_arguments) {
			CHECK_NC(TT_LBRACE);
		}

		for (;;) {

			if (TEST_NC(TT_FN)) {
				// TODO: This is the same as below
				strref name = strref::from_raw(self->token_data);
				PEEKCHECK(TT_IDENT);

				node upv = node::make_upval(self->level, narrow(binding_arr.size()));
				binding bind = { type_unit };
				binding_arr.push_back(bind);
				local loc = { name, upv };
				ADD_LOCAL(loc);

				if (HOP_PARSE) {
					expr_arr.push_back(node(self->cur - self->code.begin()));
					skip(self);
				} else {
					next(self);
					auto l = rlambda(self, TT_RBRACE, true);
					CHECK_NOTERR();

					//node_match match;
					//match.match = node::make_matchupv(upv);
					//match.value = l;
					//expr_arr.push_back(match.match);
					expr_arr.push_back(l);
					++parsed_nodes; //
				}

			} else if (TEST_NC(TT_LET)) {
				strref name = strref::from_raw(self->token_data);
				CHECK(TT_IDENT);

				node upv = node::make_upval(self->level, narrow(binding_arr.size()));
				binding bind = { type_unit };
				binding_arr.push_back(bind);
				local loc = { name, upv };
				ADD_LOCAL(loc);

				if (TEST_NC(TT_COLON)) {
					rtype(self);
				}

				if (TEST_NC(TT_EQ)) {

					FLUSH_PARSES();

					//node_match match;
					//match.match = node::make_matchupv(upv);
					//match.value = rexpr(self);
					//expr_arr.push_back(match.match);
					auto val = rexpr(self);
					expr_arr.push_back(val);
					++parsed_nodes;
				}

			} else if (self->tt != TT_EOF) {
				FLUSH_PARSES();

				node v = rexpr(self);
				expr_arr.push_back(v);
				++parsed_nodes;
			}

			if (!TEST_NC(TT_COMMA))
				break;
		}

		FLUSH_PARSES();

		self->allow_comma_in_context = old_allow_comma;

		SCOPE_END();
	}
	////
	PEEKCHECK(end_token);

	u32 lambda_off = narrow(self->output.size());
	lambda_op* lambda = (lambda_op *)self->output.unsafe_alloc(binding_arr.size_bytes() + expr_arr.size_bytes());
	u32 argc = narrow(expr_arr.size_bytes()) / sizeof(node); // TODO: What type will the expressions have?
	u32 bindings = narrow(binding_arr.size_bytes()) / sizeof(binding);

	u8* writep = (u8 *)lambda->data;
	writep += copy_to(binding_arr, writep);
	copy_params(self, expr_arr, (node *)writep);

	assert(argc < 0xffff && bindings < 0xffff);
	node v = node::make_ref(NT_LAMBDA, lambda_off, argc + (bindings << 16));

	return v;
}

#if 0
static type_node typeof_node(parser* self, node n) {

	TL_UNUSED(self);
	switch (n.type()) {
	case NT_CALL: {

		break;
	}
	}
}
#endif

// a
static node rprimary_del(parser* self) {
	node err_ret(0);
	switch (self->tt) {
		case TT_LITERALINT:
			return kint32(self, (i32)self->token_data);

		case TT_IDENT:
			return FIND_LOCAL(strref::from_raw(self->token_data));

		case TT_LPAREN: {
			next(self, false);
			bool old_allow_comma = self->allow_comma_in_context;
			self->allow_comma_in_context = false;
			node v = rexpr(self);
			self->allow_comma_in_context = old_allow_comma;
			PEEKCHECK(TT_RPAREN);
			return v;
		}

		case TT_LBRACE: {
			//next(self, false);
			return rlambda(self, TT_RBRACE, true);
		}
	}

	ERR();
}

static node flush_call(parser* self, node f, small_node_array& param_arr) {
	u32 poff = REF;
	call_op* call = (call_op *)self->output.unsafe_alloc(sizeof(call_op) + param_arr.size_bytes());
	//call->type = type_unit; // TODO: Resolve function type
	call->fun = f;
	u32 argc = narrow(param_arr.size());

	copy_params(self, param_arr, call->args);
	param_arr.clear();

	return node::make_ref(NT_CALL, poff, argc);
}

static node rsimple_expr_tail(parser* self, node r) {
	node err_ret(0);
	small_node_array param_arr;

	do {
		if (self->tt == TT_IDENT) {
			param_arr.push_back(r);
			// param_arr.size() == 1 here
			
			r = rname(self);
		}

		if (TEST_NC(TT_LPAREN)) {
			rrecord_body(self, param_arr);
			CHECK(TT_RPAREN);
		}

		if (self->tt == TT_LBRACKET || self->tt == TT_LBRACE) {
			node p = rprimary_del(self);
			CHECK_NOTERR();

			param_arr.push_back(p);
		}

		r = flush_call(self, r, param_arr);
	} while (self->tt == TT_IDENT || self->tt == TT_LPAREN);

	return r;
}

// a()
static node rsimple_expr(parser* self) {
	node err_ret(0);
	node r = rprimary_del(self);
	CHECK_NOTERR();

	if (self->tt == TT_IDENT || self->tt == TT_LPAREN) {
		return rsimple_expr_tail(self, r);
	} else {
		return r;
	}
}

// + b()
static node rexpr_rest(parser* self, node lhs, int min_prec) {
	while (self->tt == TT_OP) {
		int prec = self->token_prec;
		if (prec < min_prec) break;

		strref op = strref::from_raw(self->token_data);

		next(self, false);

		node rhs = rsimple_expr(self);

		while (self->tt == TT_OP) {
			int prec2 = self->token_prec;
			if (prec2 < prec) break;
			rhs = rexpr_rest(self, rhs, prec2);
		}

		u32 poff = REF;
		call_op* call = (call_op *)self->output.unsafe_alloc(sizeof(call_op) + sizeof(node)*2);

		call->fun = FIND_LOCAL(op);
		//call->type = type_unit;
		call->args[0] = lhs;
		call->args[1] = rhs;
		lhs = node::make_ref(NT_CALL, poff, 2);
	}

	return lhs;
}

// a() + b()
static node rexpr(parser* self) {

	node lhs = rsimple_expr(self);
	return rexpr_rest(self, lhs, 0);
}

module parser::parse_module() {
	module m;

	m.root = rlambda(this, TT_EOF);

	return move(m);
}

}
