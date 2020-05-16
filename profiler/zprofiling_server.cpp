#include "zprofiling_server.h"

#include <core/io/tcp_server.h>
#include <core/os/file_access.h>
#include <core/os/thread.h>

#include <cstring>

const char *ZProfilingServer::DEFAULT_HOST_ADDRESS = "127.0.0.1";

namespace {
ZProfilingServer *g_harvester = nullptr;
}

void ZProfilingServer::create_singleton() {
	CRASH_COND(g_harvester != nullptr);
	g_harvester = memnew(ZProfilingServer);
}

void ZProfilingServer::destroy_singleton() {
	CRASH_COND(g_harvester == nullptr);
	memdelete(g_harvester);
}

ZProfilingServer::ZProfilingServer() {
	//printf("Creating profiling server singleton\n");
	_running = true;
	_thread = Thread::create(c_thread_func, this);
}

ZProfilingServer::~ZProfilingServer() {
	//printf("Destroying profiling server singleton\n");
	_running = false;
	Thread::wait_to_finish(_thread);
	memdelete(_thread);
	//printf("Destroyed profiling server singleton\n");
}

void ZProfilingServer::c_thread_func(void *userdata) {
	ZProfilingServer *h = (ZProfilingServer *)userdata;
	h->thread_func();
}

void ZProfilingServer::thread_func() {
	ZProfiler::get_thread_profiler().set_profiler_name("ProfilingServer");

	//printf("Profiling server thread started\n");

	_server.instance();
	// Only listen to localhost
	const Error err = _server->listen(DEFAULT_HOST_PORT, IP_Address(DEFAULT_HOST_ADDRESS));
	CRASH_COND(err != OK);

	while (_running) {
		OS::get_singleton()->delay_usec(LOOP_PERIOD_USEC);

		ZProfiler::get_thread_profiler().mark_frame();
		VOXEL_PROFILE_SCOPE();

		// TODO The data should be dropped past some amount, otherwise it will saturate memory and bandwidth

		update_server();
		harvest();
		serialize_and_send_messages();
		recycle_data();
	}

	clear();

	_peer.unref();
	_server->stop();
	_server.unref();

	//printf("Profiling server stopped\n");
}

void ZProfilingServer::update_server() {
	VOXEL_PROFILE_SCOPE();

	if (_peer.is_null() && _server->is_connection_available()) {
		_peer = _server->take_connection();
		_peer_just_connected = true;
		printf("Peer connected to profiler\n");
	}

	if (_peer.is_null()) {
		return;
	}

	StreamPeerTCP &peer = **_peer;
	const StreamPeerTCP::Status peer_status = peer.get_status();

	switch (peer_status) {
		case StreamPeerTCP::STATUS_ERROR:
		case StreamPeerTCP::STATUS_NONE:
			printf("Peer disconnected from profiler\n");
			_peer.unref();
			printf("Last peer disconnected, disabling profiler\n");
			ZProfiler::set_enabled(false);
			break;

		case StreamPeerTCP::STATUS_CONNECTING:
			break;

		case StreamPeerTCP::STATUS_CONNECTED: {
			const size_t available_bytes = peer.get_available_bytes();

			if (available_bytes == 1) {
				const uint8_t command = peer.get_8();

				switch (command) {
					case CMD_ENABLE:
						printf("Enabling profiler\n");
						ZProfiler::set_enabled(true);
						break;

					case CMD_DISABLE:
						printf("Disabling profiler\n");
						ZProfiler::set_enabled(false);
						break;

					default:
						CRASH_NOW_MSG("Unhandled command ID");
						break;
				}
			}
		} break;

		default:
			CRASH_NOW_MSG("Unhandled status");
			break;
	}
}

void ZProfilingServer::harvest() {
	// Gather last recorded data.
	// This function does as few work as possible to minimize synchronization overhead,
	// so we'll have to process the data a little bit before sending it.
	ZProfiler::Buffer *harvested_buffers = ZProfiler::harvest(_recycled_buffers);
	_recycled_buffers = nullptr;

	ZProfiler::Buffer *buffer = harvested_buffers;
	while (buffer != nullptr) {
		_buffers_to_send.push_back(buffer);
		CRASH_COND(buffer->prev == buffer);
		buffer = buffer->prev;
	}
}

void ZProfilingServer::serialize_and_send_messages() {
	if (_peer.is_valid()) {
		serialize_and_send_messages(**_peer, _peer_just_connected);
		_peer_just_connected = false;
	}
}

template <typename Peer_T>
inline void serialize_string_def(Peer_T &peer, uint16_t id, String str) {
	//print_line(String("Sending string def {0}: {1}").format(varray(id, str)));
	peer.put_u8(ZProfilingServer::EVENT_STRING_DEF);
	peer.put_u16(id);
	peer.put_utf8_string(str);
}

uint16_t ZProfilingServer::get_or_create_c_string_id(const char *cs) {
	uint16_t id;
	const uint16_t *id_ptr = _static_strings.getptr(cs);
	if (id_ptr != nullptr) {
		id = *id_ptr;

	} else {
		id = _next_string_id++;
		_static_strings.set(cs, id);
		serialize_string_def(_message, id, cs);
	}
	return id;
}

uint16_t ZProfilingServer::get_or_create_string_id(String s) {
	uint16_t id;
	const uint16_t *id_ptr = _dynamic_strings.getptr(s);
	if (id_ptr != nullptr) {
		id = *id_ptr;

	} else {
		id = _next_string_id++;
		_dynamic_strings.set(s, id);
		serialize_string_def(_message, id, s);
	}
	return id;
}

void ZProfilingServer::serialize_and_send_messages(StreamPeerTCP &peer, bool send_all_strings) {
	VOXEL_PROFILE_SCOPE();

	// An intermediary buffer is used instead of `StreamPeerTCP` directly,
	// because using the latter slowed it down horribly.
	// Turning on Nagle's algorithm didn't help...
	_message.clear();

	_message.put_u8(EVENT_INCOMING_DATA_SIZE);
	const size_t incoming_data_size_index = _message.size();
	_message.put_u32(0);
	const size_t begin_index = _message.size();

	if (send_all_strings) {
		// New clients need to get all strings they missed
		{
			const String *key = nullptr;
			while ((key = _dynamic_strings.next(key))) {
				serialize_string_def(_message, _dynamic_strings.get(*key), *key);
			}
		}
		{
			const char *const *key = nullptr;
			while ((key = _static_strings.next(key))) {
				serialize_string_def(_message, _static_strings.get(*key), *key);
			}
		}
	}

	// Simple event stream.
	// Iterate buffers in reverse because that's how buffers got harvested
	for (int i = _buffers_to_send.size() - 1; i >= 0; --i) {
		ZProfiler::Buffer *buffer = _buffers_to_send[i];
		const uint16_t thread_name_id = get_or_create_string_id(buffer->thread_name);

		// Get frame buffer corresponding to the thread
		Frame *frame_buffer;
		Frame **frame_buffer_ptr = _frame_buffers.getptr(thread_name_id);
		if (frame_buffer_ptr == nullptr) {
			frame_buffer = memnew(Frame);
			_frame_buffers.set(thread_name_id, frame_buffer);
		} else {
			frame_buffer = *frame_buffer_ptr;
		}

		for (size_t j = 0; j < buffer->write_index; ++j) {
			const ZProfiler::Event &event = buffer->events[j];

			switch (event.type) {
				case ZProfiler::EVENT_PUSH: {
					const uint16_t description_id = get_or_create_c_string_id(event.description);
					++frame_buffer->current_lane;
					ERR_FAIL_COND(frame_buffer->current_lane > frame_buffer->lanes.size()); // Stack too deep?
					frame_buffer->lanes[frame_buffer->current_lane].push_back(Item{ event.relative_time, 0, description_id });
				} break;

				case ZProfiler::EVENT_PUSH_SN: {
					StringName &sn = *(StringName *)event.description_sn;
					const uint16_t description_id = get_or_create_string_id(sn);
					++frame_buffer->current_lane;
					ERR_FAIL_COND(frame_buffer->current_lane > frame_buffer->lanes.size()); // Stack too deep?
					frame_buffer->lanes[frame_buffer->current_lane].push_back(Item{ event.relative_time, 0, description_id });
					sn.~StringName();
				} break;

				case ZProfiler::EVENT_POP:
					ERR_FAIL_COND(frame_buffer->current_lane == -1); // Can't pop?
					frame_buffer->lanes[frame_buffer->current_lane].back().end = event.relative_time;
					--frame_buffer->current_lane;
					break;

				case ZProfiler::EVENT_FRAME:
					// Last frame is now finalized, serialize it

					_message.put_u8(EVENT_FRAME);
					_message.put_u16(thread_name_id);
					_message.put_u64(event.time); // This is the *end* time of the frame (and beginning of next one)

					for (size_t i = 0; i < frame_buffer->lanes.size(); ++i) {
						std::vector<Item> &items = frame_buffer->lanes[i];
						_message.put_u32(items.size());
						if (items.size() == 0) {
							break;
						}
						_message.put_data((const uint8_t *)items.data(), items.size() * sizeof(Item));
					}

					frame_buffer->reset();
					break;

				default:
					CRASH_NOW_MSG("Unhandled event type");
					break;
			}
		}

		// Optimized way to reset since we took care of freeing StringNames
		buffer->reset_no_string_names();
	}

	// Write block data size back at the beginning
	_message.set_u32(incoming_data_size_index, _message.size() - begin_index);

	{
		// Send in one block
		VOXEL_PROFILE_SCOPE();
		//print_line(String("Sending {0} bytes").format(varray(_message.size())));
		Error err = peer.put_data(_message.data(), _message.size());
		if (err != OK) {
			// The peer can have disconnected in the meantime
			ERR_PRINT("Failed to send profiling data");
		}
	}

	_message.clear();
}

void ZProfilingServer::recycle_data() {
	// Recycle buffers
	for (size_t i = 0; i < _buffers_to_send.size(); ++i) {
		ZProfiler::Buffer *buffer = _buffers_to_send[i];
		CRASH_COND(buffer == _recycled_buffers);
		buffer->prev = _recycled_buffers;
		_recycled_buffers = buffer;
	}

	_buffers_to_send.clear();
}

void ZProfilingServer::clear() {
	// Clear all data we were about to send
	for (size_t i = 0; i < _buffers_to_send.size(); ++i) {
		ZProfiler::Buffer *buffer = _buffers_to_send[i];
		memdelete(buffer);
	}

	// Clear all data we were about to recycle
	while (_recycled_buffers != nullptr) {
		ZProfiler::Buffer *buffer = _recycled_buffers->prev;
		memdelete(_recycled_buffers);
		_recycled_buffers = buffer;
	}

	// Clear frame buffers
	const uint16_t *key = nullptr;
	while ((key = _frame_buffers.next(key))) {
		Frame *f = _frame_buffers.get(*key);
		memdelete(f);
	}
	_frame_buffers.clear();

	_dynamic_strings.clear();
	_static_strings.clear();
	_buffers_to_send.clear();
	_next_string_id = 0;
}
