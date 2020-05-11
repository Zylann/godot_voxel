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
	printf("Creating profiling server singleton\n");
	_running = true;
	_thread = Thread::create(c_thread_func, this);
}

ZProfilingServer::~ZProfilingServer() {
	printf("Destroying profiling server singleton\n");
	_running = false;
	Thread::wait_to_finish(_thread);
	memdelete(_thread);
	printf("Destroyed profiling server singleton\n");
}

void ZProfilingServer::c_thread_func(void *userdata) {
	ZProfilingServer *h = (ZProfilingServer *)userdata;
	h->thread_func();
}

void ZProfilingServer::thread_func() {
	ZProfiler::get_thread_profiler().set_profiler_name("ProfilingServer");

	printf("Profiling server thread started\n");

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

	printf("Profiling server stopped\n");
}

void ZProfilingServer::update_server() {
	VOXEL_PROFILE_SCOPE();

	if (_peer.is_null() && _server->is_connection_available()) {
		_peer = _server->take_connection();
		_peer_just_connected = true;
		printf("Peer connected\n");
	}

	if (_peer.is_null()) {
		return;
	}

	StreamPeerTCP &peer = **_peer;
	const StreamPeerTCP::Status peer_status = peer.get_status();

	switch (peer_status) {
		case StreamPeerTCP::STATUS_ERROR:
		case StreamPeerTCP::STATUS_NONE:
			printf("Peer disconnected\n");
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

inline void serialize_string_def(StreamPeerTCP &peer, uint16_t id, String str) {
	//print_line(String("Sending string def {0}: {1}").format(varray(id, str)));
	peer.put_u8(ZProfilingServer::EVENT_STRING_DEF);
	peer.put_u16(id);
	peer.put_utf8_string(str);
}

void ZProfilingServer::serialize_and_send_messages(StreamPeerTCP &peer, bool send_all_strings) {
	VOXEL_PROFILE_SCOPE();

	if (send_all_strings) {
		// New clients need to get all strings they missed
		for (auto it = _dynamic_strings.begin(); it != _dynamic_strings.end(); ++it) {
			serialize_string_def(peer, it->second, it->first.c_str());
		}
		for (auto it = _static_strings.begin(); it != _static_strings.end(); ++it) {
			serialize_string_def(peer, it->second, it->first);
		}
	}

	// Simple event stream.
	// Iterate buffers in reverse because that's how buffers got harvested
	for (int i = _buffers_to_send.size() - 1; i >= 0; --i) {
		const ZProfiler::Buffer *buffer = _buffers_to_send[i];

		uint16_t thread_name_id;
		auto it = _dynamic_strings.find(buffer->thread_name);
		if (it != _dynamic_strings.end()) {
			thread_name_id = it->second;

		} else {
			// Generate string ID
			thread_name_id = _next_string_id++;
			_dynamic_strings.insert(std::make_pair(buffer->thread_name, thread_name_id));
			serialize_string_def(peer, thread_name_id, buffer->thread_name.c_str());
		}

		peer.put_u8(EVENT_THREAD);
		peer.put_u16(thread_name_id);

		for (size_t j = 0; j < buffer->write_index; ++j) {
			const ZProfiler::Event &event = buffer->events[j];
			// TODO It seems this part struggles sending large amounts of data.
			// Is it due to using small serialization functions? Or TCP can't keep up?

			switch (event.type) {
				case ZProfiler::EVENT_PUSH: {
					uint16_t description_id;
					auto it = _static_strings.find(event.description);
					if (it != _static_strings.end()) {
						description_id = it->second;

					} else {
						// Generate string ID
						description_id = _next_string_id++;
						_static_strings.insert(std::make_pair(event.description, description_id));
						serialize_string_def(peer, description_id, event.description);
					}

					peer.put_u8(EVENT_PUSH);
					peer.put_u16(description_id);
					peer.put_u32(event.time);
				} break;

				case ZProfiler::EVENT_POP:
					peer.put_u8(EVENT_POP);
					peer.put_u32(event.time);
					break;

				case ZProfiler::EVENT_FRAME:
					peer.put_u8(EVENT_FRAME);
					peer.put_u32(event.time);
					break;

				default:
					CRASH_NOW_MSG("Unhandled event type");
					break;
			}
		}
	}
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

	_dynamic_strings.clear();
	_static_strings.clear();
	_buffers_to_send.clear();
	_next_string_id = 0;
}
