#include "soio.hpp"

namespace soio {

State::State()
	: prev_obj(0), cur_sweep_bit(0) {

	this->object_proto = Value(make_object());
	this->global_obj = Value(make_object(this->object_proto));
	this->semicolon_sym = this->symbol(";"_S);
	this->braces_sym = this->symbol("{}"_S);
	this->equal_sym = this->symbol("="_S);
	this->set_slot_sym = this->symbol("setSlot"_S);
	this->empty_sym = this->symbol(""_S);
}

Value State::symbol(tl::StringSlice str) {
	u8 const* data = str.begin();
	u8 const* end = str.end();
	u8 const* p = data;
	u32 keyw = 0;
	u32 h = 0;
	while (p != end) {
		keyw = (keyw << 8) + *p;
		h = ((h << 17) | (h >> (32-17))) + keyw;
		++p;
	}

	u32 len = (u32)(end - data);

	u32 prel_offset = (u32)this->symbol_data.size();
	u32 sym_offs;
	this->symbol_set.source = tl::VecSlice<u8 const>(this->symbol_data.begin(), this->symbol_data.end());

	if (!this->symbol_set.get(h, data, prel_offset, len, &sym_offs)) {
		u8* dest = this->symbol_data.unsafe_alloc(len);
		memcpy(dest, data, len);
	}

	return Value::make_sym(sym_offs, len);
}

Value id_activate(Message* /*m*/, Value /*locals*/, Value target, Value /*self*/, State* /*state*/) {
	return target;
}

void obj_mark(Obj* obj, u32 cur_sweep_bit) {
	State::mark(obj->proto, cur_sweep_bit);
	for (auto& s : obj->slots) {
		State::mark(s.k, cur_sweep_bit);
		State::mark(s.v, cur_sweep_bit);
	}
}

Tag const value_tags[KiObject] = {
	{ State::perform, id_activate, 0 }, // Null
	{ State::perform, id_activate, 0 }, // True
	{ State::perform, id_activate, 0 }, // False
	{ State::perform, id_activate, 0 }, // F64
	{ State::perform, id_activate, 0 }, // Symbol
};

Value block_activate(Message* m, Value locals, Value target, Value self, State* state) {
	Block* block = ((Block *)target.obj);
	Obj* new_locals = state->make_object(block->context);

	Message* arg = m->first_child;
	u32 idx = 0;
	while (arg) {
		Value val = state->perform_with_locals(arg, locals);

		if (idx < block->arg_names.size()) {
			// Set local
			new_locals->slots.push_back(Obj::Slot(block->arg_names[idx], val));
		}

		arg = arg->next_child;
		++idx;
	}

	return state->perform_on(block->body, Value(new_locals), Value(new_locals));
}

Value block_make(Message* m, Value locals, Value target, Value self, State* state) {
	Block* block = state->make_block(0, locals);

	Message* args = m->first_child;

	// (body) or (params, body)
	assert(args);

	if (args->next_child) {
		if (args->first_child) { // (x, ...)
			Message* arg = args->first_child;
			do {
				block->arg_names.push_back(Value(arg->name));
				arg = arg->next_child;
			} while (arg);
		} else { // x
			Message* arg = args;
			block->arg_names.push_back(Value(arg->name));
		}

		block->body = args->next_child;
	} else {
		block->body = args;
	}

	return Value(block);
}

Value obj_set_slot(Message* m, Value locals, Value target, Value self, State* state) {
	Message* arg = m->first_child;

	if (self.kind != KiObject) {
		// TODO: Error
		abort();
	}

	// TODO: Check number of arguments
	Value key = state->perform_with_locals(arg, locals);
	arg = arg->next_child;
	Value val = state->perform_with_locals(arg, locals);

	self.obj->set_slot(key, val);

	return val;
}

Value obj_then(Message* m, Value locals, Value target, Value self, State* state) {

	if (is_true(self)) {
		// TODO: Check number of arguments
		state->perform_with_locals(m->first_child, locals);
		return Value(true);
	}

	return Value(false);
}

Value obj_else(Message* m, Value locals, Value target, Value self, State* state) {

	if (!is_true(self)) {
		// TODO: Check number of arguments
		return state->perform_with_locals(m->first_child, locals);
	}

	return self;
}

void State::sweep() {
	Obj** pp = &this->prev_obj;

	while (*pp) {
		Obj* prev = *pp;
		Obj** next_pp = &prev->prev;
		if ((prev->flags & 1) != this->cur_sweep_bit) {
			Obj* next_p = *next_pp;
			prev->~Obj();
			free(prev);
			*pp = next_p;
		} else {
			pp = next_pp;
		}
	}
}

void State::mark_roots() {
	this->cur_sweep_bit ^= 1;
	mark(this->global_obj);
	mark(this->object_proto);
	mark(this->string_proto);
	mark(this->number_proto);
}

void State::gc() {
	mark_roots();
	sweep();
}

void block_mark(Obj* block, u32 cur_sweep_bit) {
	obj_mark(block, cur_sweep_bit);

	State::mark(Value(((Block *)block)->body), cur_sweep_bit);
	State::mark(((Block *)block)->context, cur_sweep_bit);
}

Block* State::make_block(Message* body, Value context) {
	void *p = malloc(sizeof(Block));
	Block* m = register_obj(new (p) Block);

	static Tag const block_tag = {
		State::perform,
		block_activate,
		block_mark
	};

	m->t = &block_tag;
	m->proto = object_proto;
	m->body = body;
	m->context = context;
	return m;
}

void message_mark(Obj* block, u32 cur_sweep_bit) {
	obj_mark(block, cur_sweep_bit);

	State::mark(Value(((Message *)block)->next), cur_sweep_bit);
	State::mark(Value(((Message *)block)->next_child), cur_sweep_bit);
	State::mark(Value(((Message *)block)->first_child), cur_sweep_bit);
	State::mark(((Message *)block)->literal, cur_sweep_bit);
}

static Tag const message_tag = {
	State::perform,
	id_activate,
	message_mark
};

Message* State::make_message(Message* target, Value name, Message* first_child) {
	void *p = malloc(sizeof(Message));

	Message* m = register_obj(new (p) Message(first_child));

	if (target)
		target->next = m;
	assert(name.kind == KiSymbol);
	m->name = name.symbol;
	m->t = &message_tag;
	m->proto = object_proto;
	m->literal = Value::null();
	return m;
}

Message* State::make_message_lit(Message* target, Value literal) {
	void *p = malloc(sizeof(Message));

	Message* m = register_obj(new (p) Message(0));

	if (target)
		target->next = m;
	m->t = &message_tag;
	m->proto = object_proto;
	m->literal = literal;
	return m;
}

static Tag const obj_tag = {
	State::perform, // TEMP
	id_activate,
	obj_mark
};

Obj* State::make_object(Value proto) {
	void *p = malloc(sizeof(Obj));

	Obj* m = register_obj(new (p) Obj);

	m->t = &obj_tag;
	m->proto = proto;

	return m;
}

/*
String* State::make_string(tl::String value) {
	String* m = register_obj(new String(move(value)));

	m->t = &obj_tag;
	m->proto = this->string_proto;

	return m;
}*/

Value str_len(Message* m, Value locals, Value target, Value self, State* state) {

	String* str = (String *)self.obj;

	return Value((f64)str->size);
}

Value State::perform(Message* m, Value locals, Value target, State* state) {

	auto p = target;
	Value slot_value;
	while (true) {
		slot_value = find_slot(Value(m->name), p);
		if (!is_null(slot_value))
			break;

		p = state->proto(p);
		if (is_null(p)) {
			return Value();
		}
	}

	return tag(slot_value)->activate(m, locals, slot_value, target, state);
}

// Parser

struct Parser {
	Parser(State& state, u8 const* cur) : state(state), cur(cur) {

		memset(lextable, 0, sizeof(lextable));

		this->lextable[';'] = 1;
		this->lextable['='] = 2;
		this->lextable[':'] = 2;
		this->lextable['|'] = 2;
		this->lextable['&'] = 3;
		this->lextable['<'] = 4;
		this->lextable['>'] = 4;
		this->lextable['+'] = 5;
		this->lextable['-'] = 5;
		this->lextable['*'] = 6;
		this->lextable['/'] = 6;
		this->lextable['\\'] = 6;

		this->tt = TEof;
		next();
	}

	enum Token {
		TSymbol,
		TLiteral,
		TLParen,
		TRParen,
		TLBrace,
		TRBrace,
		TComma,
		TBar,
		TOp,
		TEof,
		TError
	};

	/*
	
	moo(
		x
		y
	)

	*/

	void next() {
	redo:
		ch = *cur++;

		if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
			if ((ch == '\r' || ch == '\n') && (this->tt != TLParen && this->tt != TComma)) {
				this->tt = TComma;
			} else {
				goto redo;
			}
		} else if (ch == '(') {
			this->tt = TLParen;
		} else if (ch == ')') {
			this->tt = TRParen;
		} else if (ch == '|') {
			this->tt = TBar;
		} else if (ch == '{') {
			this->tt = TLBrace;
		} else if (ch == '}') {
			this->tt = TRBrace;
		} else if (ch == ',') {
			this->tt = TComma;
		} else if (ch == '\0') {
			this->tt = TEof;
		} else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || this->lextable[ch]) {
			u8 const* start = cur - 1;

			bool is_op = this->lextable[ch] != 0;
			this->token_prec = this->lextable[ch];

			do {
				ch = *cur;
				if (is_op ? this->lextable[ch] : (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
					++cur;
				} else {
					break;
				}
			} while (true);

			auto sym = state.symbol(tl::StringSlice(start, cur));

			this->tt = is_op ? TOp : TSymbol;
			this->literal = sym;
		} else if ((ch >= '0' && ch <= '9')) {
			u8 const* start = cur - 1;
			u32 num = (ch - '0');
			do {
				ch = *cur;
				if ((ch >= '0' && ch <= '9')) {
					++cur;
					num = (num * 10) + (ch - '0');
				} else {
					break;
				}
			} while (true);

			this->tt = TLiteral;
			this->literal = Value(f64(num));
		} else if (ch == '"') {
			
			u32 cap = 8, size = 0;
			String* str = (String *)malloc(sizeof(String) + cap);
			
			while (true) {
				ch = *cur++;
				if (ch == '"' || !ch) {
					break;
				}

				str->data[size++] = ch;
				if (size == cap) {
					cap *= 2;
					str = (String *)realloc(str, sizeof(String) + cap);
				}
			}

			str = new (str) String();

			str->t = &obj_tag;
			str->proto = state.string_proto;
			str->size = size;

			this->tt = TLiteral;
			this->literal = Value(state.register_obj(str));
		} else {
			this->tt = TError;
		}
	}

	void check(Token t) {
		if (this->tt != t) {
			// Error
		}
		next();
	}

	bool test(Token t) {
		if (this->tt == t) {
			next();
			return true;
		}

		return false;
	}

	Message* r_message() {
		Message* lhs = r_expr2();
		return r_expr_rest(lhs, 0);
	}

	Message* r_expr_rest(Message* lhs, i32 min_prec) {

		Message* prev_lhs = lhs->last();

		while (this->tt == TOp) {
			i32 prec = this->token_prec;
			if (prec < min_prec) break;

			Value op = this->literal;
			next();

			Message* rhs = r_expr2();
			Message* prev_rhs = rhs;

			while (this->tt == TOp) {
				i32 prec2 = this->token_prec;
				if (prec2 < prec) break;
				prev_rhs = r_expr_rest(prev_rhs, prec2);
			}

			if (op == state.equal_sym) {
				Message* last = prev_lhs;
				Message* key_msg = state.make_message_lit(0, Value(last->name));
				key_msg->next_child = rhs;

				last->name = state.set_slot_sym.symbol;
				last->literal = Value();
				last->first_child = key_msg;

			} /*else if (op == state.semicolon_sym) {
				Message* prev = lhs->last();
				Message* semi = state.make_message(prev, op, 0);
				semi->next = rhs;
			}*/ else {
				prev_lhs = state.make_message(prev_lhs, op, rhs);
			}
		}

		return lhs;
	}

	Message** r_params(Message* m) {
		Message** prev_next = &m->first_child;

		if (this->tt != TRParen) {
			Message* p = r_message();
			*prev_next = p;
			prev_next = &p->next_child;
			while (test(TComma)) {
				p = r_message();
				*prev_next = p;
				prev_next = &p->next_child;
			}
		}

		return prev_next;
	}

	bool is_nesting_end() const {
		return this->tt == TRParen || this->tt == TRBrace || this->tt == TBar;
	}

	Message** r_tuple(Message*& prev, Message** prev_next = 0, bool force_tuple = false) {
		
		if (!is_nesting_end()) {
			Message* p = r_message();
			
			if (this->tt != TComma && !prev && !force_tuple) {
				prev_next = &p->first_child;
				prev = p;
				return prev_next;
			} else {
				// Tuple with more than one element
				if (!prev) {
					prev = this->state.make_message(0, this->state.empty_sym, 0);
				}

				if (!prev_next) {
					prev_next = &prev->first_child;
				}

				assert(p);
				*prev_next = p;
				prev_next = &p->next_child;

				// TODO: Allow ) or } for single-element tuple?
				if (test(TComma) && !is_nesting_end()) {
					do {
						p = r_message();
						assert(p);
						*prev_next = p;
						prev_next = &p->next_child;
					} while (test(TComma));
				}

				return prev_next;
			}
		}

		assert(prev); // TODO: Empty tuple
		return prev_next ? prev_next : &prev->first_child;
	}

	Message* r_expr2() {
		Message* m = 0;
		Message* prev = 0;
		Message** prev_next_child = 0;

		do {
			if (test(TLParen)) {
				
				prev_next_child = r_tuple(prev);
				check(TRParen);
			} else if (this->tt == TSymbol) {
				Message* msg = state.make_message(prev, this->literal, 0);
				prev_next_child = &msg->first_child;
				next();
				prev = msg;
			} else if (this->tt == TLiteral) {
				prev = state.make_message_lit(prev, this->literal);
				prev_next_child = &prev->first_child;

				next();
			} else if (this->tt == TLBrace) {
				Message* block = state.make_message(0, state.braces_sym, 0);
				next();
				Message** block_next_param = &block->first_child;
				if (test(TBar)) {
					Message* params = 0;
					r_tuple(params, 0, true);
					*block_next_param = params;
					block_next_param = &params->next_child;
					check(TBar);
				}
				r_tuple(block, block_next_param);
				check(TLBrace);

				if (prev_next_child) {
					*prev_next_child = block;
				} else {
					prev = block;
				}
				prev_next_child = &block->next_child;
			} else {
				break;
			}

			if (!m) m = prev;
		} while (true);

		assert(m);
		return m;
	}

	State& state;
	Value literal;
	u16 lextable[256];

	u8 const* cur;
	Token tt;
	i32 token_prec;
	u8 ch;
};

Message* State::parse(char const* str) {
	Parser parser(*this, (u8 const*)str);

	Message* root = 0;
	parser.r_tuple(root);
	return root;
}

void print_sym(State* state, SymbolRef ref) {

	for (u32 i = 0; i < ref.len; ++i) {
		printf("%c", state->symbol_data[ref.offset + i]);
	}
}

void print_value(State* state, Value val) {
	if (val.kind == KiF64) {
		printf("%f", val.f);
	} else if (val.kind == KiSymbol) {
		printf("'");
		print_sym(state, val.symbol);
		printf("'");
	} else if (val.kind == KiTrue) {
		printf("True");
	} else if (val.kind == KiFalse) {
		printf("False");
	} else if (val.kind == KiNull) {
		printf("Null");
	} else {
		printf("??");
	}
}

void print_tree(Message* m, State* state) {
	while (true) {
		if (!is_null(m->literal)) {
			print_value(state, m->literal);
		} else {
			print_sym(state, m->name);
			Message* child = m->first_child;
			if (child) {
				printf("(");
				while (child) {
					print_tree(child, state);
					child = child->next_child;
					if (child) { printf(", "); }
				}
				printf(")");
			}
		}

		m = m->next;
		if (!m) break;

		printf(" ");
	}
}

Value pm_activate(Message* m, Value locals, Value target, Value self, State* state) {
	Message* start = m->first_child;
	print_tree(start, state);
	return Value();
}

Value eq_activate(Message* m, Value locals, Value target, Value self, State* state) {
	Message* arg1 = m->first_child;
	if (arg1) {
		Message* arg2 = arg1->next_child;
		if (arg2) {
			Value a1 = state->perform_with_locals(arg1, locals);
			Value a2 = state->perform_with_locals(arg2, locals);
			return Value(a1 == a2);
		}
	}

	return Value(false);
}


Value fail_activate(Message* m, Value locals, Value target, Value self, State* state) {
	if (m->first_child) {
		printf("Failure because:");

		Value arg1 = state->perform_with_locals(m->first_child, locals);
		print_value(state, arg1);
		printf("\n");
	} else {
		printf("Failure!\n");
	}
	
	return Value();
}

Value seq_activate(Message* m, Value locals, Value target, Value self, State* state) {
	Message* arg = m->first_child;
	Value last_value;
	
	while (arg) {
		last_value = state->perform_with_locals(arg, locals);
		arg = arg->next_child;
	}

	return last_value;
}

void State::reg(Value target, Binding bindings[]) {
	for (u32 i = 0; bindings[i].func; ++i) {
		Binding binding = bindings[i];
		Tag* tag = (Tag *)malloc(sizeof(Tag));
		
		tag->activate = binding.func;
		tag->perform = State::perform;
		tag->mark = obj_mark;

		Obj* f = this->make_object();
		f->t = tag;

		target.obj->slots.push_back(
			soio::Obj::Slot(this->symbol(binding.key), Value(f)));
	}
}

void State::init() {
	
	Binding global_meth[] = {
		{ ""_S, seq_activate },
		{ "{}"_S, block_make },
		{ "pm"_S, pm_activate },
		{ "fail"_S, fail_activate },
		{ tl::StringSlice(), 0 }
	};

	Binding obj_meth[] = {
		{ "setSlot"_S, obj_set_slot },
		{ "then"_S, obj_then },
		{ "else"_S, obj_else },
		{ "=="_S, eq_activate },
		{ tl::StringSlice(), 0 }
	};

	Binding str_meth[] = {
		{ "len"_S, str_len },
		{ tl::StringSlice(), 0 }
	};

	reg(this->global_obj, global_meth);
	reg(this->object_proto, obj_meth);

	this->string_proto = Value(this->make_object());
	reg(this->string_proto, str_meth);

	this->number_proto = Value(this->make_object(this->object_proto));
	//reg(this->number_proto, num_meth);

	this->global_obj.obj->set_slot(this->symbol("True"_S), Value(true));
	this->global_obj.obj->set_slot(this->symbol("False"_S), Value(false));

	this->protos[KiTrue] = this->object_proto;
	this->protos[KiFalse] = this->object_proto;
	this->protos[KiF64] = this->number_proto;
}

}

namespace fse {
void test();
}

namespace rans { void test(); }

namespace vectest {
void test();
}

int main() {

	//vectest::test();
	//fse::test();
	//rans::test();

#if 1
	soio::State s;

	s.init();

	//soio::Message* m = s.parse("pm(id + 2 * id {a,b,a+b})");
	//soio::Message* m = s.parse("pm(if(x) { 1 } else { 2 })");
	//soio::Message* m = s.parse("pm((x) then { x = 1 } else { 2 })");
	//soio::Message* m = s.parse("(x = { |y| y }, x(2))");
	//soio::Message* m = s.parse("\"abc\" len");
	soio::Message* m = s.parse("(assert = { |x, s| x else(fail(s)) }, assert(1 == 2, \"Oops\"))");
	//soio::Message* m = s.parse("1 == 2");
	//soio::Message* m = s.parse("pm((x = { |x| 2, 3, 4 }, x))");
	//soio::Message* m = s.parse("False else(True)");
		
	soio::Value v = s.call(m);
	s.gc();
	soio::print_value(&s, v);
#endif
}