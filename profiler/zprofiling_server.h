#ifndef ZPROFILING_SERVER_H
#define ZPROFILING_SERVER_H

#include "zprofiler.h"
#include "zprofiling_send_buffer.h"

#include <core/hash_map.h>
#include <core/reference.h>
// Used for now because Godot 3.x doesn't have a worthy vector for frequently re-used memory
#include <vector>

class Thread;
class TCP_Server;
class StreamPeerTCP;

// Gathers data logged by thread profilers and sends it to network peers.
class ZProfilingServer {
public:
	static const char *DEFAULT_HOST_ADDRESS;
	static const uint16_t DEFAULT_HOST_PORT = 13118;
	static const uint64_t LOOP_PERIOD_USEC = 20000;
	static const uint32_t MAX_LANES = 64; // = maximum stack depth

	// Input commands
	enum CommandType {
		CMD_ENABLE = 0, // Turn on profiling
		CMD_DISABLE // Turn off profiling
	};

	// Output messages
	enum EventType {
		EVENT_FRAME, // Following data is about a whole frame
		EVENT_STRING_DEF, // New string ID definition
		// TODO Do we need a THREAD_END message?
		EVENT_INCOMING_DATA_SIZE // Header for incoming data block
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
	uint16_t get_or_create_c_string_id(const char *cs);
	uint16_t get_or_create_string_id(String s);

	struct Item {
		// This layout must match what the client expects
		uint32_t begin;
		uint32_t end;
		uint16_t description_id;
	};

	struct Frame {
		int current_lane = -1;
		// Vectors contain re-usable memory which is frequently pushed to
		std::array<std::vector<Item>, MAX_LANES> lanes;

		inline void reset() {
			for (size_t i = 0; i < lanes.size(); ++i) {
				if (lanes[i].size() == 0) {
					break;
				}
				lanes[i].clear();
			}
			current_lane = -1;
		}
	};

	Ref<TCP_Server> _server;
	Ref<StreamPeerTCP> _peer;
	bool _peer_just_connected = false;
	Thread *_thread = nullptr;
	bool _running = false;
	Vector<ZProfiler::Buffer *> _buffers_to_send;
	ZProfiler::Buffer *_recycled_buffers = nullptr;
	ZProfilingSendBuffer _message;

	// Re-used buffers to pre-process raw events before sending them
	HashMap<uint16_t, Frame *> _frame_buffers;

	// Strings are separated in two categories because one incurs higher performance cost
	HashMap<const char *, uint16_t> _static_strings;
	HashMap<String, uint16_t> _dynamic_strings;
	uint16_t _next_string_id = 0;
};

#endif // ZPROFILING_SERVER_H
