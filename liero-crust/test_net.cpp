#include "tl/socket.hpp"
#include "tl/string.hpp"
#include "tl/bits.h"
#include "tl/std.h"
#include "tl/memory.h"
#include "tl/region.hpp"
#include "tl/thread.hpp"

#include <memory>
#include "tl/io.hpp"
#include "tl/windows/miniwindows.h"
#include "tl/io/async.hpp"

struct Key {
	static usize const Size = 16;

	u64 v[2];

	Key() = delete;

	struct uninit_tag {};

	Key(uninit_tag) {}

	Key(u8 const* p) {
		v[0] = tl::read_le<u64>(p);
		v[1] = tl::read_le<u64>(p + 8);
	}

	Key(u64 v0, u64 v1) {
		v[0] = v0;
		v[1] = v1;
	}

	static Key random() {
		Key k((uninit_tag()));
		tl_crypto_rand(&k.v, sizeof(k.v));
		return k;
	}

	Key dist(Key other) const {
		return Key(v[0] ^ other.v[0], v[1] ^ other.v[1]);
	}

	i32 logdist(Key other) const {
		u64 v1 = v[1] ^ other.v[1];
		return v1 ? 64 + tl_top_bit64(v1) : tl_top_bit64(v[0] ^ other.v[0]);
	}

	bool operator<(Key const& other) const {
		return v[1] < other.v[1] || (v[1] == other.v[1] && v[0] < other.v[0]);
	}

	bool operator==(Key const& other) const {
		return v[0] == other.v[0] && v[1] == other.v[1];
	}

	bool operator!=(Key const& other) const {
		return !this->operator==(other);
	}
};

struct Contact {
	Contact()
		: node_id(Key::uninit_tag()), _padding(0) {
	}

	Contact(tl::InternetAddr const& addr, Key node_id)
		: node_id(Key::uninit_tag()), _padding(0) {

		this->port = addr.port();
		this->ipv4 = addr.ip4();
		this->node_id = node_id;
	}

	Key node_id; // Little endian
	u32 ipv4; // Little endian
	u16 port; // Little endian
	u16 _padding;
};

static_assert(sizeof(Contact) == 16 + 8, "Expected no extra padding in Contact");

struct KBucket {
	tl::Vec<Contact> contacts;
};

struct Node {
	TL_NON_COPYABLE(Node);
	TL_MOVABLE(Node);

	static int const K = 128;
	static int const Alpha = 5;
	Key own_node_id;
	KBucket kbuckets[K];

	/*
	own = 0100
	key = 0110
	dist = 1

	buckets = [
		[],
		[0111, 0110]
		[],
		[]
	]

	*/

	Node() = delete;

	Node(Key node_id)
		: own_node_id(node_id) {
	}

	static Node create() {
		return Node(Key::random());
	}

	void insert_contact(Contact const& contact) {
		assert(contact.node_id != this->own_node_id);
		auto bucket_index = this->own_node_id.logdist(contact.node_id);

		auto& contact_list = kbuckets[bucket_index].contacts;
		for (auto& c : contact_list) {
			if (c.node_id == contact.node_id) {
				// Already there. TODO: Bump it.
				return;
			}
		}

		// Not in list, add it.
		contact_list.push_back(contact);
	}

	usize insert_sort_contact(tl::VecSlice<Contact> const& ret, Key key, Contact const& new_contact, usize count) {
		auto cur_dist = new_contact.node_id.dist(key);
		usize check_pos = count;
		usize maximum = ret.size();

		while (check_pos > 0
			&& cur_dist < ret[check_pos - 1].node_id.dist(key)) {

			if (check_pos < maximum)
				ret[check_pos] = ret[check_pos - 1];
			--check_pos;
		}

		if (check_pos < maximum) {
			ret[check_pos] = new_contact;
			++count;
		}

		return count;
	}

	usize fill_with_closest(i32 bucket_index, Key key, usize count, tl::VecSlice<Contact> const& ret) {
		KBucket& bucket = kbuckets[bucket_index];
		usize maximum = ret.size();

		for (auto& c : bucket.contacts) {
			count = insert_sort_contact(ret, key, c, count);
			/*
			auto cur_dist = c.node_id.dist(key);
			usize check_pos = count;

			while (check_pos > 0
				&& cur_dist < ret[check_pos - 1].node_id.dist(key)) {

				if (check_pos < maximum)
					ret[check_pos] = ret[check_pos - 1];
				--check_pos;
			}

			if (check_pos < maximum) {
				ret[check_pos] = c;
				++count;
			}*/
		}

		return count;
	}

	usize find_nodes(Key key, tl::VecSlice<Contact> const& ret) {
		auto bucket_index = key.logdist(own_node_id);
		if (bucket_index < 0) bucket_index = 0;
		usize count = 0;
		usize maximum = ret.size();

		if (bucket_index >= 0) {
			for (auto cur = bucket_index; bucket_index >= 0 && count < maximum; --bucket_index) {
				count = this->fill_with_closest(bucket_index, key, count, ret);
			}

			for (auto cur = bucket_index + 1; cur < K && count < maximum; ++cur) {
				count = this->fill_with_closest(cur, key, count, ret);
			}
		}

		return count;
	}
};

template<typename T = tl::IocpRecvFromOp*>
struct Continuation {
	virtual ~Continuation() {}
	virtual bool cont(T x) = 0;
};

template<typename F, typename T = tl::IocpRecvFromOp*>
struct ContinuationFunc : Continuation<T> {
	F f;

	ContinuationFunc(F f)
		: f(move(f)) {
	}

	virtual bool cont(T x) {
		return f(x);
	}
};

struct Operation {
	Key id;
	std::unique_ptr<Continuation<>> continuation;

	Operation(Key id, Continuation<>* continuation)
		: id(id), continuation(continuation) {
	}
};

struct NodeNetworking;

#if 0
struct Operation2 {
	enum Type : u8{
		IterativeFindNodeReply
	};

	Key id;

	union {

	};

	Type type;

	bool cont(NodeNetworking& self, tl::IocpRecvFromOp* ev) {
		switch (type) {
		case IterativeFindNodeReply:
			if (ev) {
				auto* reply = (FindNodeReply const*)((RpcPacket *)ev->buffer)->payload();
				if (reply->validate_length(ev->size - RpcPacket::HeaderSize)) {

					for (u32 i = 0; i < reply->count; ++i) {
						// reply->contacts
					}
				}

				printf("[%p] Reply!\n", &self);
			} else {
				printf("[%p] Timed out :(\n", &self);
			}
			break;
		}

		return false;
	}

	Operation2(Key id)
		: id(id) {
	}
};
#endif

struct RpcPacket {
	static usize const HeaderSize = 16 + 1;

	enum Op : u8 {
		Reply = 1 << 7,
		FindNode = 0
	};

	u8 data[];

	Key& id() {
		return *(Key *)data;
	}

	u8& op() {
		return *(data + 16);
	}

	u8* payload() {
		return data + HeaderSize;
	}
};

struct FindNodeReply {
	u32 count;
	u32 _padding;
	Contact contacts[];

	FindNodeReply(tl::VecSlice<Contact> contacts) {
		this->count = contacts.size();
		for (usize i = 0; i < this->count; ++i) {
			this->contacts[i] = contacts[i];
		}
	}

	bool validate_length(usize len) const {
		return len >= 4 && len >= size(this->count);
	}

	static usize size(usize contact_count) {
		return 4 + 4 + contact_count * sizeof(Contact);
	}
};

static_assert(sizeof(FindNodeReply) == 4 + 4, "Expected no extra padding in FindNodeReply");

struct NodeNetworking {
	tl::Iocp iocp;
	tl::Socket socket;
	tl::Vec<Operation> current_operations;
	Node node;

	NodeNetworking(tl::Socket socket)
		: iocp(tl::Iocp::create())
		, socket(socket)
		, node(Node::create()) {

		iocp.reg(this->socket.to_handle());
	}

	NodeNetworking(tl::Socket socket, Key const& key)
		: iocp(tl::Iocp::create())
		, socket(socket)
		, node(key) {

		iocp.reg(this->socket.to_handle());
	}

	template<typename T, typename F>
	Key func(F f) {
		Continuation<T>* p = new ContinuationFunc<F, T>(move(f));
		Key id = Key::random();
		current_operations.push_back(Operation(id, p));
		return id;
	}

	void rpc(Contact const& contact, tl::IocpSendToOpPtr packet, Key const* id = NULL) {
		RpcPacket* p = (RpcPacket *)packet->buffer;
		p->id() = id ? *id : Key::random();

		printf("[%p] Rpc\n", this);

		auto to = tl::InternetAddr::from_ip4_port(contact.ipv4, contact.port);
		iocp.sendto(socket, packet.release(), to);
	}

	void iterative_find_node(Key key/*, unique_ptr<Continuation<tl::VecSlice<Contact const>>> cb*/) {
		Contact contacts[Node::Alpha];
		auto count = node.find_nodes(key, tl::VecSlice<Contact>(contacts, contacts + Node::Alpha));

		auto f = func<tl::IocpRecvFromOp*>([self = this/*, cb { move(cb) }*/](tl::IocpRecvFromOp* ev) {
			if (ev) {
				auto* reply = (FindNodeReply const*)((RpcPacket *)ev->buffer)->payload();

				if (reply->validate_length(ev->size - RpcPacket::HeaderSize)) {
					//cb->cont(tl::VecSlice<Contact const>(reply->contacts, reply->contacts + reply->count));
				}

				printf("[%p] Reply!\n", self);
			} else {
				printf("[%p] Timed out :(\n", self);
			}

			return false;
		});

		for (usize i = 0; i < count; ++i) {
			auto& contact = contacts[i];
			
			auto packet = iocp.new_send_to_op_ptr();

			RpcPacket* p = (RpcPacket *)packet->buffer;
			p->op() = RpcPacket::FindNode;
			packet->size = RpcPacket::HeaderSize + Key::Size;
			
			rpc(contact, move(packet), &f);
		}
	}

	void handle_recv_from(Node& node, tl::IocpEvent& ev) {

		tl::IocpRecvFromOp& req = *(tl::IocpRecvFromOp *)ev.op.get();
		usize packet_size = req.size;

		if (packet_size <= RpcPacket::HeaderSize) {
			return;
		}

		RpcPacket* p = (RpcPacket *)req.buffer;
		auto op = p->op();
		if (op & RpcPacket::Reply) {
			printf("[%p] Received reply\n", this);

			Key op_id(p->id());
			for (auto& op : current_operations) {
				if (op.id == op_id) {
					if (!op.continuation->cont(&req)) {
						current_operations.remove_swap(&op);
					}
					break;
				}
			}
		} else {

			if (op == RpcPacket::FindNode && packet_size >= RpcPacket::HeaderSize + Key::Size) {
				printf("[%p] Received RPC\n", this);

				Contact contacts[Node::K];
				auto count = node.find_nodes(*(Key *)p->payload(), tl::VecSlice<Contact>(contacts, contacts + Node::K));

				//tl::Vec<u8> packet(RpcPacket::HeaderSize + FindNodeReply::size(count));
				auto packet = iocp.new_send_to_op_ptr();
				RpcPacket* reply = (RpcPacket *)packet->buffer;
				reply->op() = RpcPacket::FindNode | RpcPacket::Reply;

				// TODO: This is not aligned
				FindNodeReply* fnr = new (reply->payload(), tl::non_null()) FindNodeReply(tl::VecSlice<Contact>(contacts, contacts + count));

				/*
				fnr->count = count;
				for (usize i = 0; i < count; ++i) {
					fnr->contacts[i] = contacts[i];
				}
				*/

				packet->size = RpcPacket::HeaderSize + FindNodeReply::size(count);

				// TODO: We don't know ID of source, but that doesn't really matter
				Contact from_contact(ev.from, Key(Key::uninit_tag()));

				rpc(from_contact, move(packet), &p->id());
			}
		}
	}

	void loop() {
		tl::Vec<tl::IocpEvent> events;
		while (true) {
			if (iocp.wait(events, 1000 * 1000)) {
				for (auto& ev : events) {

					if (ev.is_read) {
						//tl::IocpRecvFromOp& req = *(tl::IocpRecvFromOp *)ev.op.get();
						handle_recv_from(node, ev);
					}
				}
				events.clear();
			} else {
				for (auto& op : current_operations) {
					if (!op.continuation->cont(NULL)) {
						current_operations.remove_swap(&op);
					}
				}
			}
		}
	}
};

tl::ThreadScoped spawn_node(Contact& contact, Contact const* bootstrap) {

	auto sock = tl::Socket::udp();
	sock.bind(0);
	auto addr = tl::InternetAddr::from_socket(sock);
	
	//auto node = Node::create();
	auto own_node_id = Key::random();

	contact.port = addr.port();
	// TEMP
	//contact.ipv4 = 127 + (1 << 24); //addr.ip4();
	contact.ipv4 = (127 << 24) + 1; //addr.ip4();
	contact.node_id = own_node_id;

	auto th = tl::ThreadScoped::create(false, [own_node_id{ move(own_node_id) }, sock, bootstrap]() mutable {
		printf("Node up\n");

		NodeNetworking net(sock, own_node_id);

		if (bootstrap) {
			net.node.insert_contact(*bootstrap);
			net.iterative_find_node(net.node.own_node_id);
		}

		net.loop();
	});

	return move(th);
}

void test_net3() {
	Contact contact1, contact2;
	auto node1th = spawn_node(contact1, NULL);
	auto node2th = spawn_node(contact2, &contact1);

	printf("Nodes spawned\n");
}

void test_net5() {
	auto iocp = tl::Iocp::create();

	auto sock1 = tl::Socket::udp(), sock2 = tl::Socket::udp();

	//sock1.nonblocking(true);
	//sock2.nonblocking(true);

	tl::InternetAddr dest;
	dest = tl::InternetAddr::from_name("localhost", 1567);
	//tl_internet_addr_init_name(&dest, "localhost", 1567);

	//tl::PendingOperation sendop, recvop;

	int r = sock1.bind(1567);
	r = sock2.bind(0);

	iocp.reg(sock1.to_handle());
	//r = sock2.sendto_async("lo"_S, dest, &sendop);

	{
		auto th = tl::ThreadScoped::create(false, [&] {
			//sock2.sendto_async("lo"_S, dest, &sendop);
			sock2.sendto("lo"_S, dest);
		});
	}

	//u8 buf[3] = {};
	//r = sock1.recvfrom_async(tl::VecSlice<u8>(buf, buf + 2), &src, &recvop);

#if 0
	uintptr_t key;
	tl::PendingOperation* op;
	u32 bytesRecv;
	while ((op = iocp.wait(1000, key, bytesRecv)) != NULL) {
		//DWORD flags;
		//::WSAGetOverlappedResult((SOCKET)key, (LPWSAOVERLAPPED)op, &bytesRecv, FALSE, &flags);
		//::GetOverlappedResult((HANDLE)key, (LPOVERLAPPED)op->s, &bytesRecv, FALSE);
		printf("sendop:%d %d %s %d\n", op == &sendop, (int)key, buf, bytesRecv);
	}
#else
	tl::Vec<tl::IocpEvent> events;
	while (iocp.wait(events, 1000)) {
		for (auto& ev : events) {
			// TODO
			//ev.read_data.push_back(0);
			//printf("recv: %s\n", ev.read_data.begin());
		}

		events.clear();
	}
#endif

	sock1.close();
	sock2.close();
}

#if 0
void test_net4() {
	auto sock1 = tl::Socket::udp(), sock2 = tl::Socket::udp();

	sock1.nonblocking(true);
	sock2.nonblocking(true);
	
	tl::InternetAddr dest, src;
	dest = tl::InternetAddr::from_name("localhost", 1567);

	tl::PendingOperation sendop, recvop;

	int r = sock1.bind(1567);
	r = sock2.bind(0);

	{
		auto th = tl::ThreadScoped::create(false, [&] {
			//auto sendEv = CreateEvent(NULL);
			auto sendEv = WSACreateEvent();
			//sock2.sendto_async("lo"_S, dest, &sendop);
			WSAEventSelect((SOCKET)sock2.s, sendEv, FD_WRITE);

			for (int i = 0; i < 2; ++i) {
				printf("Write sleep\n");
				Sleep(2000);
				
				while (true) {
					ResetEvent(sendEv);
					if (sock2.sendto("lo"_S, dest) == tl::Socket::WouldBlock) {
						printf("Waiting to send\n");
						WaitForSingleObject(sendEv, INFINITE);
					} else {
						break;
					}
				}
				//
				
				printf("Sent\n");
			}
		});

		auto th2 = tl::ThreadScoped::create(false, [&] {
			u8 buf[3] = {};

			auto recvEv = WSACreateEvent();
			WSAEventSelect((SOCKET)sock1.s, recvEv, FD_READ);
			for (int i = 0; i < 2; ++i) {
					
				while (true) {
					ResetEvent(recvEv);
					r = sock1.recvfrom(tl::VecSlice<u8>(buf, buf + 2), src);
					if (r == tl::Socket::WouldBlock) {
						printf("Waiting to recv\n");
						WaitForSingleObject(recvEv, INFINITE);
					} else {
						break;
					}
				}

				printf("%s %d\n", buf, r);
			}
		});
	}

	/*
	uintptr_t key;
	tl::PendingOperation* op;
	while ((op = iocp.wait(1000, key)) != NULL) {
		DWORD bytesRecv, flags;
		//::WSAGetOverlappedResult((SOCKET)key, (LPWSAOVERLAPPED)op, &bytesRecv, FALSE, &flags);
		::GetOverlappedResult((HANDLE)key, (LPOVERLAPPED)op, &bytesRecv, FALSE);
		printf("sendop:%d %d %s %d\n", op == &sendop, (int)key, buf, bytesRecv);
	}
	*/

	sock1.close();
	sock2.close();
}

void test_net2() {
	auto iocp = tl::Iocp::create();

	auto sock1 = tl::Socket::udp(), sock2 = tl::Socket::udp();

	//sock1.nonblocking(true);
	//sock2.nonblocking(true);

	iocp.reg(to_handle(sock1));
	iocp.reg(to_handle(sock2));

	tl::InternetAddr dest, src;
	dest = tl::InternetAddr::from_name("localhost", 1567);
	//tl_internet_addr_init_name(&dest, "localhost", 1567);

	tl::PendingOperation sendop, recvop;

	int r = sock1.bind(1567);
	r = sock2.bind(0);
	//r = sock2.sendto_async("lo"_S, dest, &sendop);

	{
		auto th = tl::ThreadScoped::create(false, [&] {
			sock2.sendto_async("lo"_S, dest, &sendop);
		});
	}

	u8 buf[3] = {};
	r = sock1.recvfrom_async(tl::VecSlice<u8>(buf, buf + 2), &src, &recvop);

	uintptr_t key;
	tl::PendingOperation* op;
	u32 bytesRecv;
	while ((op = iocp.wait(1000, key, bytesRecv)) != NULL) {
		//DWORD flags;
		//::WSAGetOverlappedResult((SOCKET)key, (LPWSAOVERLAPPED)op, &bytesRecv, FALSE, &flags);
		//::GetOverlappedResult((HANDLE)key, (LPOVERLAPPED)op->s, &bytesRecv, FALSE);
		printf("sendop:%d %d %s %d\n", op == &sendop, (int)key, buf, bytesRecv);
	}

	sock1.close();
	sock2.close();
}
#endif

void test_net() {
	//tl::Sink snk(std::unique_ptr<tl::pushable_vector>(new tl::pushable_vector));
	tl::Vec<u8> snk;
	snk.push_back(0);
	snk.push_back(1);

	{
		auto sock1 = tl::Socket::udp(), sock2 = tl::Socket::udp();

		tl::InternetAddr dest, src;
		dest = tl::InternetAddr::from_name("localhost", 1567);
		//tl_internet_addr_init_name(&dest, "localhost", 1567);

		int r = sock1.bind(1567);
		r = sock2.bind(0);
		r = sock2.sendto("lo"_S, dest);

		u8 buf[3] = {};
		r = sock1.recvfrom(tl::VecSlice<u8>(buf, buf + 2), src);

		printf("%s\n", buf);
	}
}