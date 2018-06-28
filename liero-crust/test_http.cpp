#include "tl/io.hpp"
//#include "tl/windows/miniwindows.h"
#include "tl/io/async.hpp"
#include "tl/io/stream.hpp"
#include "tl/socket.hpp"
#include "tl/string.hpp"
#include "tl/thread.hpp"
#include <cmath>

void check(bool r) {
	if (!r) {
		printf(":(");
		abort();
	}
}

void test_http() {
	//tl::InternetAddr addr = tl::InternetAddr::from_name("localhost", 6667);
	tl::Iocp iocp = tl::Iocp::create();

	tl::Socket server_sock = tl::Socket::tcp();
	//server_sock.nonblocking(true);

	auto listen_thread = tl::ThreadScoped::create(false, [&] {
		check(server_sock.bind(27960));
		check(server_sock.listen());

		printf("Listening!\n");
		while (true) {
			tl::InternetAddr client_addr;
			auto client_sock = server_sock.accept(client_addr);

			iocp.post(client_sock.to_handle(), iocp.new_accept_op(client_addr));
			printf("Posted\n");
		}		
	});

	tl::Vec<tl::IocpEvent> events;

	while (true) {

		if (iocp.wait(events, 1000)) {
			for (auto& ev : events) {
				if (ev.type == tl::IocpOp::Accept) {
					auto client_sock = tl::Socket(ev.h.h);
					iocp.begin_recv(client_sock.to_handle());

					printf("Client!\n");
					
				} else if (ev.type == tl::IocpOp::Read) {
					auto const& recv_op = (tl::IocpRecvFromOp const&)*ev.op;
					auto client_sock = tl::Socket(ev.h.h);

					printf("Recv %d\n", (int)recv_op.size);

					auto* send_op = iocp.new_send_op(tl::Vec<u8>("HTTP/1.1 200 OK\r\n"
						"Content-Type: text/html; charset=UTF-8\r\n"
						"Cache-Control: private, max-age=0\r\n"
						"\r\n"
						"<html><head></head><body><h1 style=\"font-size: 200px\">mc?<h1></body></html>"_S));
					bool r = iocp.send(client_sock, send_op);
					printf("Starting send %d\n", (int)r);

				} else if (ev.type == tl::IocpOp::Write) {
					auto const& send_op = (tl::IocpSendOp const&)*ev.op;
					auto client_sock = tl::Socket(ev.h.h);

					printf("Sent %d\n", (int)send_op.buf.size());
					client_sock.close_send();
					printf("Done\n");
				}
			}

			events.clear();
		}
		
		printf("Waited...\n");
	}

	
}