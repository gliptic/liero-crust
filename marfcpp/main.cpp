#include "gfx/window.hpp"
#include "gfx/geom_buffer.hpp"
#include "gfx/GL/glew.h"
#include "gfx/GL/wglew.h"
#include "sim/object_list.hpp"
#include "tl/std.h"
#include "tl/vector.hpp"
#include "tl/rand.h"
#include "tl/stream.hpp"
#include <memory>

#include "tl/socket.h"

#include "hyp/parser.hpp"

typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

struct particle {
	tl::fvec2 pos, vel;
};

struct state {
	fixed_object_list<particle, 10000> particles;
};

void test_net() {
	tl::sink snk(std::unique_ptr<tl::pushable_vector>(new tl::pushable_vector));

	snk.put(0);
	snk.put(1);
	auto vec = snk.unwrap();

	{
		auto sock1 = tl_udp_socket(), sock2 = tl_udp_socket();

		tl_internet_addr dest, src;
		tl_internet_addr_init_name(&dest, "localhost", 1567);

		int r = tl_bind(sock1, 1567);
		r = tl_bind(sock2, 0);
		r = tl_sendto(sock2, "lo", 2, &dest);

		char buf[3] = {};
		r = tl_recvfrom(sock1, buf, 2, &src);

		printf("%s\n", buf);
	}
}

void test_gfx() {

	std::unique_ptr<gfx::common_window> win(gfx::window_create());
	win->set_mode(1024, 768, 0);
	win->set_visible(1);

	PFNWGLSWAPINTERVALEXTPROC sw = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	sw(1);

	float tringles[2 * 3] = {
		50.f, 50.f,
		100.f, 50.f,
		100.f, 100.f
	};

	gfx::geom_buffer buf;

	std::unique_ptr<state> st(new state);

	tl_xorshift xs = { 1 };

	for (int i = 0; i < 400; ++i) {
		auto* obj = st->particles.new_object();
		obj->pos.x = tl_xs_gen(&xs) * 640.0 / 4294967295.0;
		obj->pos.y = tl_xs_gen(&xs) * 480.0 / 4294967295.0;

		obj->vel.x = tl_xs_gen(&xs) * 2.0 / 4294967295.0 - 1.0;
		obj->vel.y = tl_xs_gen(&xs) * 2.0 / 4294967295.0 - 1.0;
	}

	while (true) {
		win->clear();

		/*
		glColor3d(1, 0, 0);

		//glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);

		glVertexPointer(2, GL_FLOAT, 0, tringles);

		glDrawArrays(GL_TRIANGLES, 0, 3);

		glDisableClientState(GL_VERTEX_ARRAY);
		//glDisableClientState(GL_COLOR_ARRAY);
		*/

		buf.points();
		buf.color(1, 1, 1, 1);
		//buf.vertex(tl::fvec2(tringles[0], tringles[1]));

		auto r = st->particles.all();
		for (particle* p; r.next(p); ) {
			buf.vertex(p->pos);
			p->pos += p->vel;
		}

		TL_TIME(draw_delay, {
			buf.clear();
		});

		if (win->buttonState[kbRight]) {
			tringles[0] += 5.0f;
			tringles[2] += 5.0f;
			tringles[4] += 5.0f;
		}

		do {
			if (!win->update()) { return; }

			for (auto& ev : win->events) {
				//
			}

			win->draw_delay += draw_delay;
			win->events.clear();

		} while (!win->end_drawing());

	}
}

struct module_printer {
	hyp::parser* p;
	int indent;
	tl::vector_slice<u8> full;

	int estimated_size;

	module_printer(hyp::parser& parser)
		: p(&parser), indent(0), full(parser.output.slice()), estimated_size(0) {
	}

	void print_indent() {
		for (int i = 0; i < indent; ++i) {
			printf("  ");
		}
	}

	void print_value(hyp::node v) {
		
		switch (v.type()) {
		case hyp::node_type::NT_NAME: {
			printf("'");
			auto str = v.str();
			str.print(p->code.begin());
			printf("'");
			estimated_size += 8;
			break;
		}

		case hyp::node_type::NT_KI32: {
			printf("%d", v.value2());
			estimated_size += v.value2() < (1 << 28) ? 4 : 8;
			break;
		}

		case hyp::node_type::NT_UPVAL: {
			printf("l%u_%u", v.upv_level(), v.upv_index());
			break;
		}

		case hyp::node_type::NT_CALL: {
			auto data = full.unsafe_cut_front_in_bytes(v.ref());

			hyp::call_op const* call = data.unsafe_read<hyp::call_op>();

			print_value(call->fun);
			printf("(");

			u32 argc = v.value2();
			
			for (u32 i = 0; i < argc; ++i) {
				if (i != 0) printf(", ");
				print_value(call->args[i]);
			}
			printf(")");
			estimated_size += 8 + 4;
			break;
		}

		case hyp::node_type::NT_LAMBDA: {
			printf("{\n");
			++indent;

			auto data = full.unsafe_cut_front_in_bytes(v.ref());

			//hyp::lambda_op const* lambda = data.unsafe_read<hyp::lambda_op>();

			u32 argc = v.value2() & 0xffff;
			u32 bindings = v.value2() >> 16;

			auto bindingArr = data.unsafe_limit_size_in_bytes(bindings * sizeof(hyp::binding));
			data = data.unsafe_cut_front_in_bytes(bindings * sizeof(hyp::binding));
			auto exprArr = data.unsafe_limit_size_in_bytes(argc * sizeof(hyp::node));

			print_lambda_body(bindingArr, exprArr);
			--indent;
			print_indent();
			printf("}");
			estimated_size += 8;
			break;
		}

		default:
			printf("%u %llu", v.type(), v.str().raw());
		}
	}


	void print_lambda_body(tl::vector_slice<u8> binding_arr, tl::vector_slice<u8> expr_arr) {

		u32 local_index = 0;
		hyp::binding* b;
		while (b = binding_arr.unsafe_read<hyp::binding>()) {
			print_indent();
			printf("let l%u\n", local_index);
			++local_index;

			estimated_size += sizeof(hyp::binding);
		}

		hyp::node* n;
		while (n = expr_arr.unsafe_read<hyp::node>()) {
			print_indent();
			if (n->type() == hyp::NT_MATCHUPV) {
				hyp::node v = *expr_arr.unsafe_read<hyp::node>();
				printf("l%u_%u = ", n->upv_level(), n->upv_index());
				print_value(v);
			} else {
				print_value(*n);
			}

			printf("\n");
		}
	}

	void print_tree(hyp::module* mod) {
		print_lambda_body(mod->binding_arr.slice(), mod->expr_arr.slice());
	}
};


void hyp_build(tl::vector_slice<u8> code) {
	hyp::parser p(code);
	
	hyp::module mod = p.parse_module();

	if (p.is_err()) {
		printf("Error in parsing at: %s\n", p.cur);
	} else if (p.cur != p.code.end()) {
		printf("Error: Did not parse all input. Left: %s\n", p.cur);
	} else {
		module_printer printer(p);
#if PRINTTREE
		printer.print_tree(&mod);
#endif
		printf("Estimate packed size: %u\n", printer.estimated_size);
	}

	printf("Source size: %u\n", (u32)code.size());
	printf("Size: %u bytes\n", (u32)(p.output.size() + mod.binding_arr.size_in_bytes() + mod.expr_arr.size_in_bytes()));

	int compressed_size = 0;

	for (int i = 0; i < p.output.size_in_bytes(); ++i) {
		u8 byte = p.output.begin()[i];

		if (byte) {
			compressed_size += 1;
		}

#if PRINTTREE
		printf("%02x", byte);

		if (((i + 1) % 16) == 0) {
			printf("\n");
		}
#endif
	}

	printf("\n\n");

#if PRINTTREE
	for (int i = 0; i < p.output.size_in_bytes(); ++i) {
		u8 byte = p.output.begin()[i];

		if (byte >= 32)
			printf(" %c", byte);
		else if (!byte)
			printf("00");
		else if (byte == 1)
			printf("11");
		else
			printf("..");

		if (((i + 1) % 16) == 0) {
			printf("\n");
		}
	}
#endif

	compressed_size += 8 * ((p.output.size_in_bytes() + 63) / 64);
	printf("\n\nCondensed size: %u bytes\n", compressed_size);
}

void test_hyp() {
	char const* templ =
		"{"
		"let fooo = { | | let x = 0\n x == fooo\n looooooong (0) }\n"
		"let booo = { |x: Foo| x.y { 0 } }\n"
		"if (foo) { bar } else { baz }"
		"}\n";

	u64 bef, aft;

	printf("Building\n");
	
	tl::vector<u8> code((u8 const*)templ, strlen(templ));

	int count = 10000;
	tl::vector<u8> bigcode;
	bigcode.reserve(code.size() * count + 1);
	for (int i = 0; i < count; ++i) {
		bigcode.unsafe_set_size((i + 1) * code.size());
		memcpy(&bigcode.begin()[i * code.size()], code.begin(), code.size());
	}
	bigcode.push_back(0);
	//hyp_build(code.slice());
	bef = tl_get_ticks();
	hyp_build(bigcode.slice());
	aft = tl_get_ticks();
	printf("Parse time: %f seconds\n", (double)(aft - bef) / 10000000.0);
}

void test_hyp_fr() {
	char const* templ =
		"let fooo = { let x = y }\n"
		"let y = 1\n";

	//u32 bef, aft;

	printf("Building\n");

	tl::vector<u8> code((u8 const*)templ, strlen(templ) + 1);
	hyp_build(code.slice());
}

int main() {
	test_hyp();
	//test_hyp_fr();

	return 0;
}
