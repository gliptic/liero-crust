local net = ...

ffi.cdef [[
	typedef struct
	{
		short ss_family;
		char  ss_data[128];
	} __attribute__((aligned(8))) internet_addr;

	typedef struct
	{
        union {
                struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { unsigned short s_w1,s_w2; } S_un_w;
                unsigned S_addr;
        } S_un;
	} in_addr;

	typedef struct sockaddr_in {

		short sin_family;
		short sin_port;
		in_addr sin_addr;
		char sin_zero[8];
	} SOCKADDR_IN, *PSOCKADDR_IN;

	typedef struct sockaddr sockaddr;

	int WSAStartup(short, void*);

	uintptr_t socket(
		int af,
		int type,
		int protocol
	);
	int bind(
		uintptr_t s,
		sockaddr* addr,
		int namelen);

	int ioctlsocket(uintptr_t, long, unsigned*);
	int listen(uintptr_t s, int backlog);
	uintptr_t accept(
		uintptr_t s,
        sockaddr* addr,
        int* addrlen);

	int connect(
        uintptr_t s,
        sockaddr* name,
        int namelen);
	int WSAGetLastError(void);

	int send(uintptr_t s, const char* buf, int len, int flags);
]]

local w = ffi.load "ws2_32"

local function htons(x) return bit.rshift(bit.bswap(b), 16) end

local initialized
local function init_sockets()
	if not initialized then
		local temp = ffi.new"char[1024]"

		w.WSAStartup(0x0202, temp)
		initialized = true
	end
end

function net.s_set_nonblocking(s, state)
	local b = ffi.new("unsigned[1]", state)

	w.ioctlsocket(sock, 0x4008667E, b)
end

function net.udp_socket()
	init_sockets()

	local s = w.socket(2, 2, 0)

	if s ~= -1 then
		net.set_nonblocking(s, true)
	end
end

function net.s_bind(s,p)
	local addr = ffi.new("sockaddr_in", 2, htons(p), 0)
	ffi.fill(addr.sin_zero, 8)

	return w.bind(s, addr, ffi.sizeof"sockaddr_in") ~= -1
end

function net.s_listen(s)
	return w.listen(s,5) ~= -1
end

function net.s_accept(s,addr)
	local b = ffi.new("int[1]",ffi.sizeof"sockaddr_in")
	return w.accept(s,addr,b)
end

function net.s_connect(s,addr)
	local r = w.connect(s,addr,ffi.sizeof"sockaddr_in")
	if r == -1 then
		local err = w.WSAGetLastError()
		return err == 10035
	end
	return true
end

local function translate_comm_ret(r)
	if r == 0 then return net.tl_disconnected
	else if r < 0 then
		local err = w.WSAGetLastError()
		if err == 10054 then return net.tl_conn_reset
		elseif err == 10035 then return net.tl_would_block
		else return net.tl_failure
		end
	end

	return r
end

function net.send(s,d,l)
	return translate_comm_ret(w.send(s, d, l, 0))
end

return net