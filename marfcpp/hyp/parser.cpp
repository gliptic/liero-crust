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

#define CH (*code)
#define NEXT (++code)
#define CHECKEND (void)0

static void next(parser* self);
static void rexpr_p(parser* self);
static node rexpr(parser* self);

static void next(parser* self) {
	u8 ch;
	u8 const* code = self->code;
redo:
	ch = CH;
	u32 lexcat = self->lextable[ch];

	NEXT;

	if (lexcat & (LEX_OP | LEX_BEGIDENT)) {
		// TODO: Reserve string buffer here
		u32 keyw = ch;

		u8* ident_beg = self->strset.buf.end() + 4;
		u32 len = 1;

		assert(TL_IS_ALIGNED(ident_beg, 4));

		u32 cont_mask = (lexcat & LEX_OP) ? LEX_OP : LEX_INNERIDENT;

		u32 h = keyw;
		*ident_beg = ch;

		while (self->lextable[ch = CH] & cont_mask) {
			keyw = (keyw << 8) + ch;
			//h = (h * 33) ^ keyw;
			h = ((h << 17) | (h >> (32-17))) + keyw;
			ident_beg[len++] = ch;
			NEXT;
		}

		CHECKEND;

		// 28 bits to use for string reference
		// 0xxx a*8 b*8 c*8
		// 1 i*27

		// For 64 bit we would have 60 bits for reference:
		// 0xxx a*8 b*8 c*8 d*8 e*8 f*8 g*8
		// 1xxx x*28 i*28
		//
		// This allows a length field of 31 bits and an offset of 28 + 2 = 30 bits

		if (len < 4) {
			self->token_data = keyw | (len << 24);
		} else {
			h *= 2 * h + 1;
			if (!h) h = 1;

			u32 ref;

			if (!self->strset.get(h, ident_beg, len, &ref)) {
				*(u32*)(ident_beg - 4) = len;
				self->strset.buf.v.size += 4 + TL_ALIGN_SIZE(len, 4);
			}

			// Strings are aligned to 4 bytes, so we can use up to 29 bits
			assert(ref < (1 << 29));
			self->token_data = (ref >> 2) | (1 << 27);
		}

		if (self->token_data == str_short1('=')) {
			self->tt = TT_EQ;
		} else if (self->token_data == str_short3('l','e','t')) {
			self->tt = TT_LET;
		} else {
			self->tt = (lexcat & LEX_OP) ? TT_OP : TT_IDENT;
			self->token_prec = lexcat >> 16;
		}
	} else if (lexcat & LEX_SINGLECHAR) {
		self->tt = lexcat >> 16;
	} else if (lexcat & LEX_WHITESPACE) {
		goto redo;
	} else { // if (lexcat & LEX_DIGIT) {
#if 1
		char const* start = (char const*)code - 1;

		u32 num = ch - '0';
		while (self->lextable[ch = CH] & LEX_DIGIT) {
			num = (num * 10) + (ch - '0');
			NEXT;
		}

		CHECKEND;

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

	self->code = code;
}

#undef NEXT
#undef CH

#define PEEKCHECK(t) if (self->tt != (t)) { self->tt = TT_ERR; return err_ret; } else (void)0
#define CHECK(t) { PEEKCHECK(t); next(self); }
#define TEST(t) (self->tt == (t) && (next(self), 1))

parser::parser(u8 const* c)
	: level(0) {
	
	for (u8 i = 0; ++i != 0;) this->lextable[i] = LEX_SINGLECHAR | (TT_ERR << 16);

	this->lextable[0] = LEX_SINGLECHAR | (TT_EOF << 16);

	for (u8 i = 'a'; i <= 'z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 'A'; i <= 'Z'; ++i) this->lextable[i] = LEX_BEGIDENT | LEX_INNERIDENT;
	for (u8 i = 128; i != 0; ++i) this->lextable[i] = LEX_INNERIDENT;
	for (u8 i = '0'; i <= '9'; ++i) this->lextable[i] = LEX_INNERIDENT | LEX_DIGIT;

	this->lextable['\''] = LEX_BEGIDENT;

	this->lextable['.'] = LEX_BEGIDENT;
	this->lextable[' '] = LEX_WHITESPACE;
	this->lextable['\r'] = LEX_WHITESPACE;
	this->lextable['\n'] = LEX_WHITESPACE;

	this->lextable['{'] = LEX_SINGLECHAR + (TT_LBRACE << 16);
	this->lextable['}'] = LEX_SINGLECHAR + (TT_RBRACE << 16);
	this->lextable['('] = LEX_SINGLECHAR + (TT_LPAREN << 16);
	this->lextable[')'] = LEX_SINGLECHAR + (TT_RPAREN << 16);
	this->lextable['['] = LEX_SINGLECHAR + (TT_LBRACKET << 16);
	this->lextable[']'] = LEX_SINGLECHAR + (TT_RBRACKET << 16);
	this->lextable[':'] = LEX_SINGLECHAR + (TT_COLON << 16);
	this->lextable[';'] = LEX_SINGLECHAR + (TT_SEMICOLON << 16);
	this->lextable[','] = LEX_SINGLECHAR + (TT_COMMA << 16);
	this->lextable['|'] = LEX_SINGLECHAR + (TT_BAR << 16);

	this->lextable['='] = LEX_OP + (0 << 16);
	this->lextable['&'] = LEX_OP + (0 << 16);
	this->lextable['<'] = LEX_OP + (1 << 16);
	this->lextable['>'] = LEX_OP + (1 << 16);
	this->lextable['+'] = LEX_OP + (2 << 16);
	this->lextable['-'] = LEX_OP + (2 << 16);
	this->lextable['*'] = LEX_OP + (3 << 16);
	this->lextable['/'] = LEX_OP + (3 << 16);
	this->lextable['\\'] = LEX_OP + (3 << 16);

	u32 len = (u32)strlen((char const*)c);

	this->strset.buf.reserve(len);

	//Buffer_reserve(&self->strset.buf, len);
	//Buffer_reset(&self->strset.buf, 0);

	this->code = c;
	this->end = c + len + 1;
	next(this);
}

#define REF ((u32)self->output.size() + 0)
#define DEREF(r) (self->output.begin() + (r))

static node kint32(parser* self, i32 v) {
	if (v >= -small_int_lim && v < small_int_lim)
		return node::make(NT_KI32, self->token_data);
	else {
		node n;
		n.v = 0; // TODO: Internalize constant
		return n;
	}
}

static node Parser_lexerr(parser* self) {
	self->tt = TT_ERR;
	node v = {0};
	return v;
}

static node find_local(parser* self, u32 name) {
	local* end = (local*)self->locals_buf.end();
	local* beg = (local*)self->locals_buf.begin();

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
			node local = find_local(self, self->token_data);
			next(self);

			return local;
		}

		default: {
			return node_none;
		}
	}
}

static int rlambda_body(parser* self, tl::vector<u8>& binding_arr, tl::vector<u8>& expr_arr) {
	int err_ret = 1;
	u32 index = 0;
	u32 locals_ref = self->locals_buf.size();

	++self->level;

	if (TEST(TT_BAR)) {
		if (self->tt != TT_BAR) {
			do {
				u32 name = self->token_data;
				CHECK(TT_IDENT);

				node upv = node::make_upval(self->level, index);
				local loc = { name, upv };
				self->locals_buf.push_back(loc);
				++index;

				if (TEST(TT_COLON)) {
					rtype(self);
				}

			} while (TEST(TT_COMMA));
		}

		next(self);
	}

	for (;;) {

		if (TEST(TT_LET)) {
			u32 name = self->token_data;
			CHECK(TT_IDENT);

			node upv = node::make_upval(self->level, index);
			local loc = { name, upv };
			binding bind = { 0 };
			self->locals_buf.push_back(loc);
			binding_arr.unsafe_push(bind);
			++index;

			if (TEST(TT_EQ)) {
				node_match match;
				match.match = node::make_matchupv(upv);
				match.value = rexpr(self);
				expr_arr.unsafe_push(match);
			}


		} else {
			node v = rexpr(self);
			expr_arr.unsafe_push<node>(v);
		}

		if (!TEST(TT_COMMA))
			break;
	}

	--self->level;
	self->locals_buf.v.size = locals_ref;
	return 0;
}

TL_INLINE size_t Buffer_copyTo(tl::vector<u8>& self, void* dest) {
	size_t len = self.size();
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
			return node::make_str(self->token_data);

		case TT_LPAREN: {
			next(self);
			node v = rexpr(self);
			PEEKCHECK(TT_RPAREN);
			return v;
		}

		case TT_LBRACE: {
			tl::vector<u8> binding_arr;
			tl::vector<u8> expr_arr;
			
			next(self);
			rlambda_body(self, binding_arr, expr_arr);
			PEEKCHECK(TT_RBRACE);

			u32 lambdaOff = self->output.size();
			lambda_op* lambda = (lambda_op*)self->output.unsafe_alloc(sizeof(lambda_op) + binding_arr.size() + expr_arr.size());
			lambda->argc = expr_arr.size() / sizeof(node); // TODO: What type will the expressions have?
																	// TODO: Set binding count

			u8* writep = (u8*)lambda->data;
			writep += Buffer_copyTo(binding_arr, writep);
			Buffer_copyTo(expr_arr, writep);

			node v = node::make_ref(NT_LAMBDA, lambdaOff);
			return v;
		}
	}

	node err = {0};
	return err;
}

static node rsimple_expr_tail(parser* self, node r) {
	tl::vector<u8> nodeArr;

	for (;;) {
		switch (self->tt) {
		case TT_IDENT: {
			u32 name = self->token_data;
			next(self);

			if (self->tt == TT_LPAREN) {

			} else if (self->tt == TT_LBRACE) {

			} else {
				u32 poff = REF;
				call_op* call = (call_op*)self->output.unsafe_alloc(sizeof(call_op) + sizeof(node) * 1);

				call->fun = node::make_str(name);
				call->argc = 1;
				call->args[0] = r;

				r = node::make(NT_CALL, poff);
			}

			break;
		}

		case TT_LPAREN:
		case TT_LBRACKET:
		case TT_LBRACE: {

			if (self->tt == TT_LBRACKET || self->tt == TT_LBRACE) {
				rprimary_del(self);
				next(self);
			} else {
				// TODO: Record
			}

			// TODO: Add parameter to r
			break;
		}

		default:
			return r;
		}
	}
}

// a()
static node rsimple_expr(parser* self) {
	node r = rprimary_del(self);

	next(self);

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

		u32 op = self->token_data;

		next(self);

		node rhs = rsimple_expr(self);

		while (self->tt == TT_OP) {
			int prec2 = self->token_prec;
			if (prec2 < prec) break;
			rhs = rexpr_rest(self, rhs, prec2);
		}

		u32 poff = REF;
		call_op* call = (call_op *)self->output.unsafe_alloc(sizeof(call_op) + sizeof(node)*2);

		call->fun = node::make_str(op);
		call->argc = 2;
		call->args[0] = lhs;
		call->args[1] = rhs;

		lhs = node::make(NT_CALL, poff);
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
