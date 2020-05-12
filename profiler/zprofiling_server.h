#ifndef ZPROFILING_SERVER_H
#define ZPROFILING_SERVER_H

#include "zprofiler.h"
#include "zprofiling_send_buffer.h"

#include <core/reference.h>

#include <unordered_map>
#include <vector>

class Thread;
class TCP_Server;
class StreamPeerTCP;

// Gathers data logged by thread profilers and sends it to network peers.
class ZProfilingServer {
public:
	static const char *DEFAULT_HOST_ADDRESS;
	static const uint16_t DEFAULT_HOST_PORT = 13118;
	static const uint64_t LOOP_PERIOD_USEC = 50000;

	// Input commands
	enum CommandType {
		CMD_ENABLE = 0, // Turn on profiling
		CMD_DISABLE // Turn off profiling
	};

	// Output messages
	enum EventType {
		EVENT_PUSH = 0, // Entered scope
		EVENT_POP, // Left scope
		EVENT_FRAME, // The current frame ended and a new one started
		EVENT_STRING_DEF, // Definition of a string, which will be referred to with a specified ID
		EVENT_THREAD // Following messages will be about the specified thread
		// TODO Do we need a THREAD_END message?
	};

	static void create_singleton();
	static void destroy_singleton();

	// TODO This was meant to be private. Had to make it public for `memdelete` to work...
	~ZProfilingServer();

private:
	ZProfilingServer();

	static void c_thread_func(void *userdata);

	void thread_func();
	void update_server();
	void harvest();
	void serialize_and_send_messages();
	void serialize_and_send_messages(StreamPeerTCP &peer, bool send_all_strings);
	void recycle_data();
	void clear();

	Ref<TCP_Server> _server;
	Ref<StreamPeerTCP> _peer;
	bool _peer_just_connected = false;
	Thread *_thread = nullptr;
	bool _running = false;
	std::vector<ZProfiler::Buffer *> _buffers_to_send;
	ZProfiler::Buffer *_recycled_buffers = nullptr;
	ZProfilingSendBuffer _message;

	// Strings are separated in two categories because one incurs higher performance cost
	std::unordered_map<const char *, uint16_t> _static_strings;
	std::unordered_map<std::string, uint16_t> _dynamic_strings;
	uint16_t _next_string_id = 0;
};

#endif // ZPROFILING_SERVER_H
