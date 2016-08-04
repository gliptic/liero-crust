#ifndef SOIO_HPP
#define SOIO_HPP

#include <tl/std.h>
#include <tl/string_set.hpp>
#include <tl/string.hpp>

namespace soio {

enum ValueKind : u8 {
	KiNull = 0,
	KiTrue,
	KiFalse,
	KiF64,
	KiSymbol,
	KiObject
};

struct SymbolRef {
	u32 offset;
	u32 len;

	bool operator==(SymbolRef const& other) const { return this->offset == other.offset && this->len == other.len; }
};

struct Obj;
struct Message;
struct State;

struct Value {
	ValueKind kind;
	union {
		SymbolRef symbol;
		Obj* obj;
		f64 f;
		u64 u;
	};

	static Value null() {
		return Value(KiNull);
	}

	Value()
		: kind(KiNull) {
	}

	explicit Value(SymbolRef ref)
		: kind(KiSymbol) {
		this->symbol = ref;
	}

	explicit Value(ValueKind kind)
		: kind(kind) {
	}

	explicit Value(Obj* m) {
		this->kind = KiObject;
		this->obj = m;
	}

	explicit Value(f64 f) {
		this->kind = KiF64;
		this->f = f;
	}

	explicit Value(bool b) {
		this->kind = b ? KiTrue : KiFalse;
	}

	static Value make_sym(u32 offset, u32 len) {
		Value v(KiSymbol);
		v.symbol.offset = offset;
		v.symbol.len = len;
		return v;
	}

	bool operator==(Value const& other) const {
		return this->kind == other.kind && this->u == other.u;
	}
};

inline bool is_null(Value v) { return v.kind == KiNull; }
inline bool is_true(Value v) { return v.kind != KiNull && v.kind != KiFalse; }

typedef Value (*ActivateFunc)(Message* m, Value locals, Value target, Value self, State* state);
typedef void (*MarkFunc)(Obj* obj, u32 cur_sweep_bit);


struct Tag {
	Value (*perform)(Message* m, Value locals, Value target, State* state);
	ActivateFunc activate;
	MarkFunc mark;
};

struct Obj {
	
	struct Slot {
		Slot(Value k, Value v) : k(k), v(v) {
		}

		Value k;
		Value v;
	};

	virtual ~Obj() { }

	Value find_slot(Value key) {
		for (auto& s : slots) {
			if (s.k == key) {
				return s.v;
			}
		}

		return Value();
	}

	void set_slot(Value key, Value val) {
		for (auto& s : slots) {
			if (s.k == key) {
				s.v = val;
				return;
			}
		}

		slots.push_back(Slot(key, val));
	}

	Tag const* t;
	Obj* prev;
	Value proto;
	tl::VecSmall<Slot, 16> slots;
	u32 flags;
};

extern Tag const value_tags[KiObject];

inline Tag const* tag(Value v) { return v.kind == KiObject ? v.obj->t : &value_tags[v.kind]; }

inline Value find_slot(Value key, Value target) {
	if (target.kind == KiObject) {
		return target.obj->find_slot(key);
	}

	return Value();
}

struct String : Obj {
	String() {
	}

	u32 size;
	u8 data[];
};

struct Message : Obj {
	Message(Message* first_child)
		: next(0)
		, first_child(first_child)
		, next_child(0) {
	}

	SymbolRef name;
	Message *next;
	Message *first_child, *next_child;
	Value literal;

	Message* last() {
		Message* cur = this;

		while (cur->next) cur = cur->next;
		return cur;
	}
};

struct Block : Obj {
	Message* body;
	tl::Vec<Value> arg_names;
	Value context;
};

struct Binding {
	tl::StringSlice key;
	ActivateFunc func;
};

struct State {
	Value protos[KiObject];
	Value object_proto, string_proto, number_proto;
	Value global_obj;
	Value semicolon_sym, braces_sym, equal_sym, set_slot_sym, empty_sym;
	tl::StringSet symbol_set;
	tl::Vec<u8> symbol_data;
	Obj* prev_obj;
	u8 cur_sweep_bit;

	State();

	void init();
	void reg(Value target, Binding bindings[]);
	void sweep();
	void mark_roots();
	void gc();

	inline void mark(Value v) {
		mark(v, this->cur_sweep_bit);
	}

	static inline void mark(Value v, u32 cur_sweep_bit) {
		if (v.kind == KiObject && v.obj) {
			if ((v.obj->flags & 1) != cur_sweep_bit) {
				v.obj->flags ^= 1;
				v.obj->t->mark(v.obj, cur_sweep_bit);
			}
		}
	}

	inline Value proto(Value v) { return v.kind == KiObject ? v.obj->proto : protos[v.kind]; }

	Value call(Message* m) {
		return perform_on(m, global_obj, global_obj);
	}

	Value perform_with_locals(Message* m, Value locals) {
		return perform_on(m, locals, locals);
	}

	Value perform_on(Message* m, Value locals, Value target) {
		Value top_target = target, result(KiNull);

		do {
			result = m->literal;
			if (is_null(result)) {
				result = tag(target)->perform(m, locals, target, this);
			}

			target = result;
		} while ((m = m->next) != 0);

		return result;
	}

	static Value perform(Message* m, Value locals, Value target, State* state);

	Value symbol(tl::StringSlice str);

	Obj* make_object(Value proto = Value());

	template<typename T>
	T* register_obj(T* o) {
		o->prev = this->prev_obj;
		o->flags = this->cur_sweep_bit;
		this->prev_obj = o;
		return o;
	}

	//String* make_string(tl::String value);
	Message* make_message(Message* target, Value name, Message* first_child);
	Message* make_message_lit(Message* target, Value literal);
	Block* make_block(Message* body, Value context);

	Message* parse(char const* str);
};

}

#endif // SOIO_HPP
