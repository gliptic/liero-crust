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

//


void print_value(hyp::parser* p, hyp::node v) {
	printf("%u %u\n", v.type(), v.str());
}

#define Buffer_checkread(ty, self, p) ((self)->end() - (p) >= sizeof(ty))
#define Buffer_read(ty, self, p) (((p) += sizeof(ty)), *(ty*)((p) - sizeof(ty)))

void print_lambda_body(hyp::parser* p, tl::vector<u8>* bindingArr, tl::vector<u8>* exprArr) {
	u8* ptr = bindingArr->begin();

	u32 local_index = 0;
	while (Buffer_checkread(hyp::binding, bindingArr, ptr)) {
		hyp::binding b = Buffer_read(hyp::binding, bindingArr, ptr);
		printf("let l%u\n", local_index);
	}

	ptr = exprArr->begin();
	while (Buffer_checkread(hyp::node, exprArr, ptr)) {

		hyp::node b = Buffer_read(hyp::node, exprArr, ptr);

		if (b.type() == hyp::NT_MATCHUPV) {
			hyp::node v = Buffer_read(hyp::node, exprArr, ptr);
			printf("l%u_%u = ", b.upv_level(), b.upv_index());
			print_value(p, v);
		} else {
			print_value(p, b);
		}
	}
}

void print_tree(hyp::parser* p, hyp::module* mod) {
	print_lambda_body(p, &mod->binding_arr, &mod->expr_arr);
}

void hyp_build(u8 const* code) {
	hyp::parser p(code);
	
	hyp::module mod = p.parse_module();

	if (p.code != p.end) {
		//printf("Error: Did not parse all input. %d bytes left.\n", p.end - p.code);
		printf("Error: Did not parse all input. %d bytes left.\n", (u32)(p.end - p.code));
	} else {
		print_tree(&p, &mod);
	}

	printf("Size: %u + %u bytes\n", p.output.size(), p.strset.buf.size());
}

void test_hyp() {
	char const* templ =
		"let fooo = { || let x = 0, x == 1 },"
		"let booo = { |x: Foo| x y }";

	//u32 bef, aft;

	printf("Building\n");
	//bef = getticks();
	hyp_build((u8 const*)templ);
}

int main() {
	test_hyp();

	return 0;
}