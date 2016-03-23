local net = {}

local gfx = import("libs/gfx")

local tl_failure, tl_would_block, tl_conn_reset, tl_disconnected = -1, -2, -3, -4

ffi.cdef [[
	typedef union { int _int; void* _voidp; } tl_socket;

	typedef struct tl_internet_addr tl_internet_addr;

	tl_socket tl_socket_invalid();

	int  tl_socket_is_valid(tl_socket sock);
	void tl_socket_close();

	tl_socket tl_tcp_socket();
	tl_socket tl_udp_socket();
	
	void      tl_set_nonblocking(tl_socket sock, int no_blocking);
	int       tl_set_nodelay(tl_socket sock, int no_delay);
	int       tl_bind(tl_socket sock, int port);
	int       tl_listen(tl_socket sock);
	tl_socket tl_accept(tl_socket sock, tl_internet_addr* addr);
	int       tl_connect(tl_socket sock, tl_internet_addr* addr);
	
	/* Communication */
	int tl_send(tl_socket sock, void const* msg, size_t len);
	int tl_recv(tl_socket sock, void* msg, size_t len);
	int tl_sendto(tl_socket sock, void const* msg, size_t len, tl_internet_addr const* dest);
	int tl_recvfrom(tl_socket sock, void* msg, size_t len, tl_internet_addr* src);
	
	int tl_opt_error(tl_socket sock);
	
	enum
	{
		TL_SS_MAXSIZE = 128,                 // Maximum size
		TL_SS_ALIGNSIZE = sizeof(int64_t), // Desired alignment
		TL_SS_PAD1SIZE = TL_SS_ALIGNSIZE - sizeof(short),
		TL_SS_PAD2SIZE = TL_SS_MAXSIZE - (sizeof(short) + TL_SS_PAD1SIZE + TL_SS_ALIGNSIZE)
	};

	typedef struct tl_sockaddr_storage
	{
		short ss_family;               // Address family.

		char _ss_pad1[TL_SS_PAD1SIZE];  // 6 byte pad, this is to make
									   //   implementation specific pad up to
									   //   alignment field that follows explicit
									   //   in the data structure
		int64_t _ss_align;            // Field to force desired structure
		char _ss_pad2[TL_SS_PAD2SIZE];  // 112 byte pad to achieve desired size;
									   //   _SS_MAXSIZE value minus size of
									   //   ss_family, __ss_pad1, and
									   //   __ss_align fields is 112
	} tl_sockaddr_storage;

	typedef struct tl_internet_addr
	{
		tl_sockaddr_storage storage_;
	} tl_internet_addr;

	void tl_internet_addr_init_empty(tl_internet_addr* self); // any
	void tl_internet_addr_init_name(tl_internet_addr* self, char const* name, int port);
	
	// NOTE: This may not get the address unless I/O has occured
	// on the socket or (if applicable) a connect or accept has
	// been done.
	void tl_internet_addr_init_from_socket(tl_internet_addr* self, tl_socket s);
	void tl_internet_addr_init(tl_internet_addr* self, uint32_t ip, int port);
	
	int tl_internet_addr_valid(tl_internet_addr* self);
	
	int  tl_internet_addr_port(tl_internet_addr const* self);
	void tl_internet_addr_set_port(tl_internet_addr* self, int port_new);
    
	uint32_t tl_internet_addr_ip(tl_internet_addr const* self);
	void     tl_internet_addr_set_ip(tl_internet_addr* self, uint32_t ip_new);
    
	void tl_internet_addr_reset(tl_internet_addr* self);

	int tl_internet_addr_eq(tl_internet_addr const*, tl_internet_addr const*);

	typedef struct
	{
		uint32_t len;
		uint32_t last_send_time;
		uint32_t timeout_time;
		uint8_t* cur;
		uint8_t* limit;
	} tl_packet_header;

	typedef struct
	{
		tl_packet_header hdr; // Not sent
		uint16_t serial;
		uint8_t  type;
		uint8_t  acks;
	} __attribute__((packed)) tl_packet;

	typedef struct
	{
		tl_packet common;
		uint32_t  magic;
		uint16_t  received_serial;
	} __attribute__((packed)) tl_packet_init;

	typedef struct
	{
		tl_packet common;
		uint16_t  message_desc; // LSB is 'complete' flag
		uint8_t   data[?];
	} __attribute__((packed)) tl_packet_data;

]]

local band, bor, lshift, rshift, tobit = bit.band, bit.bor, bit.lshift, bit.rshift, bit.tobit

local nswap32
if ffi.abi "be" then
	nswap32 = bit.bswap
else
	nswap32 = function(x) return x end
end

local tl_failure = -1
local tl_would_block = -2
local tl_conn_reset = -3
local tl_disconnected = -4

local n = gfx.g

local packet = ffi.typeof "tl_packet"
local packet_init = ffi.typeof "tl_packet_init"
local packet_data = ffi.typeof "tl_packet_data"
local header_size = ffi.sizeof "tl_packet_header"
local packet_data_size = ffi.sizeof("tl_packet_data", 0)

local temp_buf = ffi.new "char[4096]"

local function init_sockets(node, port)
	if node.sock then
		n.tl_socket_close(node.sock)
	end
	node.port = port
	node.sock = n.tl_udp_socket()
	n.tl_bind(node.sock, port)
end

local pt_init, pt_init_ack, pt_data = 0, 1, 2

local soft_packet_size = 600
local hard_packet_size = soft_packet_size + 16
local extra_packet_size = hard_packet_size - soft_packet_size
local resend_delay = 1000
local timeout = 5000

local cast = ffi.cast

assert(hard_packet_size > soft_packet_size + 5)

local function require_ack(p)
	local type = cast("tl_packet*", p).type
	-- If message_desc == 0, there are no messages, not even partial ones
	return type ~= pt_data or (cast("tl_packet_data*", p).message_desc ~= 0)
end

local function create_queue()
	local b = 1
	local e = 1
	return {
		-- Push nil until the queue is a certain size
		enlarge = function(self, count)
			if e - b < count then
				e = b + count
			end
		end,

		push = function(self, v)
			self[e] = v
			e = e + 1
		end,

		pop = function(self)
			assert(b <= e)
			local v = self[b]
			b = b + 1
			return v
		end,

		peek = function(self)
			assert(b <= e)
			return self[b]
		end,

		-- Index is 0-based
		get = function(self, idx)
			assert(idx >= 0 and idx < e - b)
			return self[b + idx]
		end,

		put = function(self, idx, v)
			assert(idx >= 0 and idx < e - b)
			self[b + idx] = v
		end,

		count = function(self)
			return e - b
		end
	}
end

local function read_packet(self, packet)
	if packet.common.type == pt_data then
		self.in_ready_messages = self.in_ready_messages + rshift(packet.message_desc, 1)
		self.in_ready_queue:push(packet)

		print("Ready messages: " .. self.in_ready_messages)
	end
end

local link_mt = {
	init = function(self)
		self.outgoing_ack = {}
		self.next_out = 0
		self.next_in = 0
		self.connected = false
		self.state = 'unconnected'
		self.pending_ack = {}
		self.pending_send = create_queue()
		self:new_out_packet()

		self.oldest_pending_message = 0
		self.oldest_pending_ack = 0

		self.in_ready_messages = 0
	end,

	new_out_packet = function(self)
		self.next_out_packet = packet_data(2048)
		self.next_out_packet.common.hdr.cur = self.next_out_packet.data
		-- One more byte may be written past the limit, therefore we subtract 1.
		self.next_out_packet.common.hdr.limit = self.next_out_packet.data + soft_packet_size - 1
		self.pending_messages = 0
		self.complete_messages = 0
		--
	end,

	-- TODO: The way this works, we should only write the byte after overflow
	-- if we're working with bits.
	write_byte = function(self, b)
		local out_packet = self.next_out_packet

		if (out_packet.common.hdr.limit - out_packet.common.hdr.cur) < 1 then
			-- New packet
			self:flush_out()
		end

		out_packet.common.hdr.cur[0] = b
		out_packet.common.hdr.cur = out_packet.common.hdr.cur + 1
	end,

	begin_message = function(self)
		if self.pending_messages == 0 then
			self.oldest_pending_message = gfx.timer_ms()
		end
		self.pending_messages = self.pending_messages + 1
	end,

	end_message = function(self)
		-- Count complete
		self.complete_messages = self.complete_messages + 1
	end,

	connect = function(self)
		self.next_packet_id = 0
		local packet = packet_init()
		packet.common.hdr.len = ffi.sizeof("tl_packet_init", 0)
		packet.common.type = pt_init
		packet.common.acks = 0
		self:send(packet)
	end,

	send = function(self, packet)
		if require_ack(packet) then
			packet.common.serial = self.next_out
			self.next_out = band(self.next_out + 1, 0xffff)
		else
			packet.common.serial = 0xffff -- For debugging
		end

		packet.common.hdr.timeout_time = gfx.timer_ms() + timeout

		local r = self:trysend(packet)
		if r < 0 then
			self.pending_send:push(packet)
		end
	end,

	resend = function(self, packet)
		print(self, "Resending " .. packet.common.serial)

		local r = self:trysend(packet)
		if r < 0 then
			self.pending_send:push(packet)
		end
	end,

	trysend = function(self, packet)
		
		local r = self.owner:send(self.addr, packet)

		if r >= 0 then
			print(self, "Sent packet " .. packet.common.serial .. " with acks == " .. packet.common.acks)
			if require_ack(packet) then
				print(self, "Pending ack for " .. packet.common.serial)
				self.pending_ack[#self.pending_ack + 1] = packet
			end
		elseif r == tl_failure then
			--print("Failure to send")
		end

		packet.common.hdr.last_send_time = gfx.timer_ms()
		return r
	end,

	check_resend = function(self)
		local now = gfx.timer_ms()

		for i=1,#self.pending_ack do
			local p = self.pending_ack[i]
			if p and tobit(now - p.common.hdr.last_send_time) > resend_delay then
				if tobit(now - p.common.hdr.timeout_time) >= 0 then
					-- TODO: Disconnect
					print(self, "Timeout...")
				else
					self:resend(p)
				end
			end
		end
	end,

	flush_packets = function(self)
		while self.pending_send:count() > 0 do
			local p = self.pending_send:peek()
			local r = self:trysend(p)
			if r < 0 then return end
			self.pending_send:pop()
		end
	end,

	ack_received = function(self, serial)
		print("Received ack for " .. serial)
		local count = #self.pending_ack
		for i=1,count do
			if self.pending_ack[i].common.serial == serial then
				-- Swap with last
				self.pending_ack[i] = self.pending_ack[count]
				self.pending_ack[count] = nil
				break
			end
		end
	end,

	schedule_ack = function(self, serial)
		print("Scheduling ack for " .. serial)
		local count = #self.outgoing_ack
		if count == 0 then
			self.oldest_pending_ack = gfx.timer_ms()
		end
		self.outgoing_ack[count + 1] = serial
	end,

	setup_in_queue = function(self, serial)
		self.next_in = band(serial + 1, 0xffff)
		self.in_queue = create_queue()
		self.in_ready_queue = create_queue()
	end,

	handle_packet = function(self, packet)
		local serial = packet.serial
		local packet_type = packet.type

		if packet_type == pt_init_ack then
			-- TODO: Handle pt_init_ack duplication
			local initack = ffi.cast("tl_packet_init*", packet)
			self.connected = true
			--self.next_in = band(packet.common.serial + 1, 0xffff)
			--self.in_queue = create_queue()
			self:setup_in_queue(initack.common.serial)

			self:ack_received(initack.received_serial)
			print("Got init ack")
		elseif packet_type == pt_data then
			if self.in_queue == nil then
				print("Ignoring packet " .. serial .. ": no in_queue")
				-- No in_queue
				return
			end

			local datapacket = ffi.cast("tl_packet_data*", packet)
			local data_size = datapacket.common.hdr.len - packet_data_size

			-- Process contained acks

			-- TODO: Check datapacket.common.acks * 2 <= data_size

			local ack_ptr = datapacket.data + data_size
			for i=1,datapacket.common.acks do
				local ack = bor(lshift(ack_ptr[-1], 8), ack_ptr[-2])
				self:ack_received(ack)
				ack_ptr = ack_ptr - 2
			end

			if not require_ack(datapacket) then
				print("Stop processing packet, no serial")
				return
			end

			local rel_serial = tobit(serial - self.next_in)

			if rel_serial < 0 then
				-- Old, ignore
				print("Ignoring old serial " .. serial)
			else
				self.in_queue:enlarge(rel_serial + 1)
				if type(self.in_queue:get(rel_serial)) == "nil" then
					-- New packet
					print("Got new packet " .. serial)
					local total_size = datapacket.common.hdr.len
					local copy = packet_data(total_size - packet_data_size)
					ffi.copy(copy, datapacket, total_size)

					self.in_queue:put(rel_serial, copy)

					-- TODO: Extract packets from the front

					while self.in_queue:count() > 0
					and type(self.in_queue:peek()) ~= "nil" do
						local p = self.in_queue:pop()
						self.next_in = self.next_in + 1

						read_packet(self, p)
					end
				else
					-- Already got this, ignore
					print("Already got " .. serial)
				end
			end
		end

		-- pt_init has its own ack, pt_init_ack
		if packet_type ~= pt_init and require_ack(packet) then
			self:schedule_ack(serial)
		end
	end,

	check_packet = function(self)
		local now = gfx.timer_ms()

		local out_packet = self.next_out_packet
		local out_bytes = (out_packet.data - out_packet.data) -- TODO: Doesn't count any unflushed bits
		local max_wait, max_ack_wait = 100, 100

		if out_bytes > 300 or
		  (self.pending_messages > 0 and tobit(now - self.oldest_pending_message) > max_wait) or
		  (self.outgoing_ack[1] ~= nil and tobit(now - self.oldest_pending_ack) > max_ack_wait) then

		  -- TODO: Flush partial bytes
		  self:flush_out()
		end
	end,

	flush_out = function(self)
		local out_packet = self.next_out_packet

		local message_desc = lshift(self.pending_messages, 1)
		if self.pending_messages > self.complete_messages then
			-- Set partial flag
			message_desc = bor(message_desc, 1)
		end

		-- Correct some packet parameters
		out_packet.message_desc = message_desc
		out_packet.common.type = pt_data

		-- Move limit beyond reserved space
		out_packet.common.hdr.limit = out_packet.common.hdr.limit + extra_packet_size

		-- Write acks
		local ack_count = 0
		while self.outgoing_ack[1] ~= nil and (out_packet.common.hdr.limit - out_packet.common.hdr.cur) >= 2 do
			
			local ack = table.remove(self.outgoing_ack, 1)
			out_packet.common.hdr.cur[0] = band(ack, 0xff)
			out_packet.common.hdr.cur[1] = band(rshift(ack, 8), 0xff)
			out_packet.common.hdr.cur = out_packet.common.hdr.cur + 2
			ack_count = ack_count + 1
		end

		local total_size = (out_packet.common.hdr.cur - ffi.cast("uint8_t*", out_packet))
		out_packet.common.hdr.len = total_size
		out_packet.common.acks = ack_count

		self.pending_messages = self.pending_messages - self.complete_messages

		self:send(out_packet)
		self:new_out_packet()
	end,

	process = function(self)
		if self.connected then
			-- TODO: Do not send any packets if waiting for disconnection
			self:check_packet()
		end
		--[[
			if(is_connected()) /* Don't send anything until we're connected */
			{
				if(con_state & got_ack_for_fin)
				{
					if(udiff(get_ticks(), disconnected_time) >= 0)
					{
						set_disconnected();
					}
				}
				else
				{
					packet* p = ch.check_packet(250, 300, 50);
					if(p)
						send(p);
				}
			}
		]]
		self:check_resend()
		self:flush_packets()
	end
}

link_mt.__index = link_mt

local node_mt = {
	host = function(self, port)
		init_sockets(self, port)

		self.accept_connections = true
	end,

	connect = function(self, name, port)
		init_sockets(self, 0)

		local addr = ffi.new("tl_internet_addr")
		n.tl_internet_addr_init_name(addr, name, port)

		local new_link = self:add_link(addr)
		new_link:connect()
		self.server_link = new_link

		self.accept_connections = false

		return self.server_link
	end,

	send = function(self, to, packet)
		local len = packet.common.hdr.len - header_size
		--print("Trying to send " .. len .. " bytes")
		local r = n.tl_sendto(self.sock,
			ffi.cast("char*", packet) + header_size,
			len,
			to)
		--print("Sent " .. r .. " bytes")
		return r
	end,

	add_link = function(self, addr)
		local link = setmetatable({
			owner = self,
			id = self.next_link_id,
			addr = addr,
			self.sock
		}, link_mt)

		link:init()

		self.next_link_id = self.next_link_id + 1

		self.links[#self.links + 1] = link
		return link
	end,

	process = function(self)
		while true do
			local from_addr = ffi.new("tl_internet_addr")
			local r = n.tl_recvfrom(self.sock, temp_buf + header_size, 2048, from_addr)
		
			if r == tl_would_block or r == 0 then
				break
			end
			if r < 0 then
				print("Socket error: " .. r)
				break
			end

			print("Received packet, size " .. r)

			local temp_packet = ffi.cast("tl_packet*", temp_buf)
			temp_packet.hdr.len = r + header_size
			
			local serial = temp_packet.serial
			local type = temp_packet.type

			if type == pt_init then
				-- TODO: Handle pt_init duplication
				local link = self:add_link(from_addr)

				local packet = packet_init()
				packet.common.hdr.len = ffi.sizeof("tl_packet_init", 0)
				packet.common.type = pt_init_ack
				packet.received_serial = serial
				link:send(packet)

				link.connected = true
				link:setup_in_queue(serial)

				print("Got init")
			else
				local found_link = nil
				for i=1,#self.links do
					local link = self.links[i]
					if n.tl_internet_addr_eq(link.addr, from_addr) ~= 0 then
						found_link = link
						break
					end
				end

				if found_link then
					found_link:handle_packet(temp_packet)
				end
			end
		end

		for i=1,#self.links do
			local link = self.links[i]
			link:process()
		end
	end
}

node_mt.__index = node_mt

function net.create_node()
	local node = {
		links = {},
		next_link_id = 0,
		port = 0,
		accept_connections = false
	}

	setmetatable(node, node_mt)

	return node
end

function net.test()
	local h, c = net.create_node(), net.create_node()

	local port = 31337
	h:host(port)
	local server = c:connect("localhost", port)

	server:begin_message()
	server:write_byte(0x13)
	server:write_byte(0x37)
	server:end_message()

	while true do
		h:process()
		c:process()
	end
end

--[[
local port = 31337

local host = n.tl_udp_socket()
n.tl_bind(host, port)

local host_addr = ffi.new("tl_internet_addr")
n.tl_internet_addr_init_name(host_addr, "localhost", port)

local client = n.tl_udp_socket()
n.tl_bind(client, 0)

local r = n.tl_sendto(client, "hello", 5, host_addr)
print("tl_sendto(...) = " .. r)

local buffer = ffi.new("uint8_t[2048]")
repeat
	local from_addr = ffi.new("tl_internet_addr")
	r = n.tl_recvfrom(host, buffer, 2048, from_addr)
	print(r)
until r > 0

local received = ffi.string(buffer, r)
print("Got: " .. received)
]]

return net