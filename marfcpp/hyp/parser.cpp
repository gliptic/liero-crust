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

enum {
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
	TT_EOF = 16,
	TT_ERR = 17
};

#define CH (*cur)
#define NEXT (++cur)

static void next(parser* self, bool allow_comma = true);
static node rexpr(parser* self);

static void next(parser* self, bool allow_comma) {
	u8 ch;
	u8* cur = self->cur;

	assert(self->tt != TT_ERR);

	ch = CH;
	u32 lexcat = self->lextable[ch];
	NEXT;

other:
	if (lexcat & (LEX_OP | LEX_BEGIDENT)) {

		u8* ident_beg = cur - 1;
		u32 cont_mask = (lexcat & LEX_OP) ? LEX_OP : LEX_INNERIDENT;

		u64 keyw = ch | 0x100;
		u32 h = (u32)keyw;

		while (self->lextable[ch = CH] & cont_mask) {
			keyw = (keyw << 8) + ch;
			//h = (h * 33) ^ keyw;
			h = ((h << 17) | (h >> (32-17))) + (u32)keyw;
			NEXT;
		}

		u32 len = (u32)(cur - ident_beg);
		if (len <= 7) {
			self->token_data = strref::from_raw(keyw).raw();
		} else {
			h *= 2 * h + 1;
			if (!h) h = 1;

			self->strset.source = tl::vector_slice<u8>(self->code.begin(), ident_beg);

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
		} else {
			self->tt = (lexcat & LEX_OP) ? TT_OP : TT_IDENT;
			self->token_prec = lexcat >> 16;
		}
	} else if (lexcat & LEX_SINGLECHAR) {
		self->tt = lexcat >> 16;
	} else if (lexcat & LEX_WHITESPACE) {
		do {
			if ((lexcat & LEX_NEWLINE) && allow_comma && self->allow_comma_in_context) {
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
			num = (num * 10) + (ch - '0');
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

		self->tt = TT_LITERALINT;
		self->token_data = num;
	}

done:
	//printf("T: %d\n", self->tt);

	self->cur = cur;
}

#undef NEXT
#undef CH

#define ERR() { self->tt = TT_ERR; return err_ret; }
#define PEEKCHECK(t) if (self->tt != (t)) { ERR(); } else (void)0

#define CHECK(t) { PEEKCHECK(t); next(self); }
#define CHECK_NOTERR() if (self->tt == TT_ERR) { return err_ret; } else next(self)
#define TEST(t) (self->tt == (t) && (next(self), true))

#define CHECK_NC(t) { PEEKCHECK(t); next(self, false); }
#define CHECK_NOTERR_NC() if (self->tt == TT_ERR) { return err_ret; } else next(self, false)
#define TEST_NC(t) (self->tt == (t) && (next(self, false), true))

parser::parser(tl::vector_slice<u8> c)
	: level(0), code(c), tt(TT_COMMA)
	, allow_comma_in_context(false) {
	
	for (u8 i = 0; ++i != 0;) this->lextable[i] = LEX_SINGLECHAR | (TT_ERR << 16);

	this->lextable[0] = LEX_SINGLECHAR | (TT_EOF << 16);

	for (u8 i = 'a'; i <= 'z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 'A'; i <= 'Z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 128; i != 0; ++i) this->lextable[i] = LEX_INNERIDENT;
	for (u8 i = '0'; i <= '9'; ++i) this->lextable[i] = LEX_INNERIDENT | LEX_DIGIT;

	this->lextable['\''] = LEX_BEGIDENT;

	this->lextable['.'] = LEX_BEGIDENT;
	this->lextable[' '] = LEX_WHITESPACE;
	this->lextable['\r'] = LEX_WHITESPACE | LEX_NEWLINE;
	this->lextable['\n'] = LEX_WHITESPACE | LEX_NEWLINE;

	this->lextable['{'] = LEX_SINGLECHAR + (TT_LBRACE << 16);
	this->lextable['}'] = LEX_SINGLECHAR + (TT_RBRACE << 16);
	this->lextable['('] = LEX_SINGLECHAR + (TT_LPAREN << 16);
	this->lextable[')'] = LEX_SINGLECHAR + (TT_RPAREN << 16);
	this->lextable['['] = LEX_SINGLECHAR + (TT_LBRACKET << 16);
	this->lextable[']'] = LEX_SINGLECHAR + (TT_RBRACKET << 16);
	this->lextable[':'] = LEX_SINGLECHAR + (TT_COLON << 16);
	this->lextable[';'] = LEX_SINGLECHAR + (TT_SEMICOLON << 16);
	this->lextable[','] = LEX_SINGLECHAR + (TT_COMMA << 16);

	this->lextable['='] = LEX_OP + (0 << 16);
	this->lextable['|'] = LEX_OP + (0 << 16);
	this->lextable['&'] = LEX_OP + (1 << 16);
	this->lextable['<'] = LEX_OP + (2 << 16);
	this->lextable['>'] = LEX_OP + (2 << 16);
	this->lextable['+'] = LEX_OP + (3 << 16);
	this->lextable['-'] = LEX_OP + (3 << 16);
	this->lextable['*'] = LEX_OP + (4 << 16);
	this->lextable['/'] = LEX_OP + (4 << 16);
	this->lextable['\\'] = LEX_OP + (4 << 16);

	this->cur = code.begin();
	next(this, false);
}

bool parser::is_err() {
	return this->tt == TT_ERR;
}

#define REF ((u32)self->output.size() + 0)
#define DEREF(r) (self->output.begin() + (r))

// TODO: Internalize bigger constants

static node kint32(parser* self, i32 v) {
	return node::make(NT_KI32, 0, v);
}

static node Parser_lexerr(parser* self) {
	self->tt = TT_ERR;
	node v = {0};
	return v;
}

static node find_local(parser* self, strref name) {
	local const* end = (local const*)self->locals_buf.end();
	local const* beg = (local const*)self->locals_buf.begin();

	for (; end != beg;) {
		--end;
		if (end->name == name) {
			return end->ref;
		}
	}

	return node::make_str(name);
}

static node rtype(parser* self) {
	switch (self->tt) {
		case TT_IDENT: {
			node local = find_local(self, strref::from_raw(self->token_data));
			next(self);

			return local;
		}

		default: {
			return node_none;
		}
	}
}

static void rrecord_body(parser* self, tl::vector<node>& expr_arr) {

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

static int rlambda_body(parser* self, tl::mixed_buffer& binding_arr, tl::mixed_buffer& expr_arr) {
	int err_ret = 1;
	u32 index = 0;
	u32 locals_ref = (u32)self->locals_buf.size();

	bool old_allow_comma = self->allow_comma_in_context;
	self->allow_comma_in_context = true;

	++self->level;

	if (TEST_NC(TT_BAR)) {
		if (self->tt != TT_BAR) {
			do {
				strref name = strref::from_raw(self->token_data);
				CHECK(TT_IDENT);

				node upv = node::make_upval(self->level, index);
				local loc = { name, upv };
				self->locals_buf.push_back(loc);
				++index;

				if (TEST_NC(TT_COLON)) {
					rtype(self);
				}

			} while (TEST_NC(TT_COMMA));
		}

		CHECK_NC(TT_BAR);
	}

	for (;;) {

		if (TEST_NC(TT_LET)) {
			strref name = strref::from_raw(self->token_data);
			CHECK(TT_IDENT);

			node upv = node::make_upval(self->level, index);
			local loc = { name, upv };
			binding bind = { 0 };
			self->locals_buf.push_back(loc);
			binding_arr.unsafe_push(bind);
			++index;

			if (TEST_NC(TT_COLON)) {
				// TODO: If we have a full type, allow forward references in value if it is a lambda
			}

			if (TEST_NC(TT_EQ)) {
				node_match match;
				match.match = node::make_matchupv(upv);
				match.value = rexpr(self);
				expr_arr.unsafe_push(match);
			}

		} else {
			node v = rexpr(self);
			expr_arr.unsafe_push<node>(v);
		}

		if (!TEST_NC(TT_COMMA))
			break;
	}

	self->allow_comma_in_context = old_allow_comma;

	--self->level;

	// TODO: Match [locals_ref..] with 
	self->locals_buf.unsafe_set_size(locals_ref);
	return 0;
}

template<typename T>
inline usize Buffer_copyTo(tl::vector<T>& self, void* dest) {
	usize len = self.size_in_bytes();
	memcpy(dest, self.begin(), len);
	return len;
}

// a
static node rprimary_del(parser* self) {
	node err_ret = {0};
	switch (self->tt) {
		case TT_LITERALINT:
			return kint32(self, (i32)self->token_data);

		case TT_IDENT:
			return find_local(self, strref::from_raw(self->token_data));

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
			tl::mixed_buffer binding_arr, expr_arr;
			
			next(self, false);
			rlambda_body(self, binding_arr, expr_arr);
			PEEKCHECK(TT_RBRACE);

			u32 lambdaOff = (u32)self->output.size();
			lambda_op* lambda = (lambda_op *)self->output.unsafe_alloc(binding_arr.size_in_bytes() + expr_arr.size_in_bytes());
			u32 argc = (u32)expr_arr.size_in_bytes() / sizeof(node); // TODO: What type will the expressions have?
			u32 bindings = (u32)binding_arr.size_in_bytes() / sizeof(binding);

			u8* writep = (u8 *)lambda->data;
			writep += Buffer_copyTo(binding_arr, writep);
			Buffer_copyTo(expr_arr, writep);

			node v = node::make_ref(NT_LAMBDA, lambdaOff, argc + (bindings << 16));
			return v;
		}
	}

	ERR();
}

static node flush_call(parser* self, node f, tl::vector<node>& param_arr) {
	if (!param_arr.empty()) {
		u32 poff = REF;
		call_op* call = (call_op *)self->output.unsafe_alloc(sizeof(call_op) + param_arr.size_in_bytes());
		call->fun = f;
		u32 argc = (u32)param_arr.size();
		Buffer_copyTo(param_arr, call->args);
		param_arr.clear();

		return node::make_ref(NT_CALL, poff, argc);
	} else {
		return f;
	}
}

static node rsimple_expr_tail(parser* self, node r) {
	node err_ret = {0};
	tl::vector<node> param_arr;

	for (;;) {
		switch (self->tt) {
		case TT_IDENT: {
			strref name = strref::from_raw(self->token_data);
			next(self);

			r = flush_call(self, r, param_arr);

			param_arr.push_back(r);
			r = find_local(self, name);

			if (TEST_NC(TT_LPAREN)) {
				// We need special case for this so it's interpreted as x y(...), not (x y) (...)
				rrecord_body(self, param_arr);
				CHECK(TT_RPAREN);
			}

			break;
		}

		case TT_LPAREN:
			next(self, false);
			r = flush_call(self, r, param_arr);
			rrecord_body(self, param_arr);
			CHECK(TT_RPAREN);
			break;

		case TT_LBRACKET:
		case TT_LBRACE: {
			node p = rprimary_del(self);
			CHECK_NOTERR();
			param_arr.push_back(p);

			r = flush_call(self, r, param_arr);
			break;
		}

		default:
			return flush_call(self, r, param_arr);
		}
	}
}

// a()
static node rsimple_expr(parser* self) {
	node err_ret = {0};
	node r = rprimary_del(self);
	CHECK_NOTERR();

#if 1 // TEMP disabled
	switch (self->tt) {
	case TT_IDENT:
	case TT_LPAREN:
	case TT_LBRACKET:
	case TT_LBRACE:
		return rsimple_expr_tail(self, r);

	default:
		return r;
	}
#endif

}

// + b()
static node rexpr_rest(parser* self, node lhs, int minPrec) {
	while (self->tt == TT_OP) {
		int prec = self->token_prec;
		if (prec < minPrec) break;

		strref op = strref::from_raw(self->token_data);

		next(self, false); // Allow comma: never

		node rhs = rsimple_expr(self); // Allow comma: if parent does

		while (self->tt == TT_OP) {
			int prec2 = self->token_prec;
			if (prec2 < prec) break;
			rhs = rexpr_rest(self, rhs, prec2); // Allow comma: if parent does
		}

		u32 poff = REF;
		call_op* call = (call_op *)self->output.unsafe_alloc(sizeof(call_op) + sizeof(node)*2);

		call->fun = node::make_str(op);
		call->type = 0;
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

	rlambda_body(this, m.binding_arr, m.expr_arr);

	return move(m);
}

}
