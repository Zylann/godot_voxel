#include "zprofiling_client.h"
#include "zprofiling_client_flame_view.h"
#include "zprofiling_server.h"

#include <core/io/stream_peer_tcp.h>
#include <scene/gui/box_container.h>
#include <scene/gui/button.h>
#include <scene/gui/label.h>

const uint32_t MAX_LANES = 256;

inline void set_margins(Control *c, int left, int top, int right, int bottom) {
	c->set_margin(MARGIN_LEFT, left);
	c->set_margin(MARGIN_RIGHT, right);
	c->set_margin(MARGIN_TOP, top);
	c->set_margin(MARGIN_BOTTOM, bottom);
}

ZProfilingClient::ZProfilingClient() {
	VBoxContainer *main_vb = memnew(VBoxContainer);
	main_vb->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	::set_margins(main_vb, 4, 4, -4, -4);
	add_child(main_vb);

	HBoxContainer *header_hb = memnew(HBoxContainer);
	main_vb->add_child(header_hb);

	_connect_button = memnew(Button);
	_connect_button->set_text(TTR("Connect"));
	_connect_button->connect("pressed", this, "_on_connect_button_pressed");
	header_hb->add_child(_connect_button);

	_flame_view = memnew(ZProfilingClientFlameView);
	_flame_view->set_client(this);
	_flame_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vb->add_child(_flame_view);

	HBoxContainer *footer_hb = memnew(HBoxContainer);
	main_vb->add_child(footer_hb);

	_status_label = memnew(Label);
	_status_label->set_text("---");
	footer_hb->add_child(_status_label);

	_peer.instance();

	set_process(true);
}

ZProfilingClient::~ZProfilingClient() {
}

void ZProfilingClient::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			_process();
			break;

		default:
			break;
	}
}

void ZProfilingClient::_process() {
	if (_peer.is_null()) {
		return;
	}

	const StreamPeerTCP::Status peer_status = _peer->get_status();

	if (peer_status != _previous_peer_status) {
		_previous_peer_status = peer_status;

		switch (peer_status) {
			case StreamPeerTCP::STATUS_CONNECTED:
				_status_label->set_text(TTR("Status: connected"));
				_connect_button->set_disabled(false);
				_connect_button->set_text("Disconnect");
				_peer->put_8(ZProfilingServer::CMD_ENABLE);
				break;

			case StreamPeerTCP::STATUS_CONNECTING:
				_status_label->set_text(TTR("Status: connecting..."));
				break;

			case StreamPeerTCP::STATUS_ERROR:
				_status_label->set_text(TTR("Status: error"));
				reset_connect_button();
				break;

			case StreamPeerTCP::STATUS_NONE:
				_status_label->set_text(TTR("Status: disconnected"));
				reset_connect_button();
				break;

			default:
				CRASH_NOW_MSG("Unhandled status");
				break;
		}
	}

	if (peer_status == StreamPeerTCP::STATUS_CONNECTED) {
		_process_connected();
	}
}

void ZProfilingClient::_process_connected() {
	CRASH_COND(_peer.is_null());
	StreamPeerTCP &peer = **_peer;

	if (peer.get_available_bytes() < 1) {
		return;
	}

	const uint8_t event_type = peer.get_u8();

	switch (event_type) {
		case ZProfilingServer::EVENT_PUSH: {
			if (_last_received_thread_id == -1) {
				// TODO Maybe we could just ignore and wait until the next frame starts?
				disconnect_on_error("Thread ID not received yet (Push)");
				return;
			}
			const uint16_t description_id = peer.get_u16();
			const String *string_ptr = _strings.getptr(description_id);
			if (string_ptr == nullptr) {
				disconnect_on_error(String("Push string {0} was not registered").format(varray(description_id)));
				return;
			}
			const uint32_t event_time = peer.get_u32();

			ThreadData &thread_data = _threads.write[_last_received_thread_id];
			if (thread_data.frames.size() == 0) {
				Frame f;
				f.begin_time = event_time;
				thread_data.frames.push_back(f);
			}
			Frame &frame = thread_data.frames.write[thread_data.frames.size() - 1];
			CRASH_COND(frame.begin_time == -1);

			++thread_data.current_lane_index;

			if (thread_data.current_lane_index > MAX_LANES) {
				// Can happen with stack overflows, or unclosed profiling scopes
				disconnect_on_error("Too many Push received");
				return;
			}

			if (thread_data.current_lane_index >= frame.lanes.size()) {
				// Must always grow by single increments
				CRASH_COND(frame.lanes.size() != thread_data.current_lane_index);
				frame.lanes.push_back(Lane());
			}

			Lane &lane = frame.lanes.write[thread_data.current_lane_index];
			if (lane.items.size() > 0) {
				const Item &last_item = lane.items[lane.items.size() - 1];
				if (last_item.end_time == -1) {
					disconnect_on_error("Received a Push but the lane has a non-terminated item");
					return;
				}
			}

			Item item;
			item.begin_time = event_time;
			item.description_id = description_id;
			lane.items.push_back(item);
		} break;

		case ZProfilingServer::EVENT_POP: {
			if (_last_received_thread_id == -1) {
				// TODO Maybe we could just ignore and wait until the next frame starts?
				disconnect_on_error("Thread ID not received yet (Pop)");
				return;
			}

			const uint32_t event_time = peer.get_u32();

			ThreadData &thread_data = _threads.write[_last_received_thread_id];
			Frame &frame = thread_data.frames.write[thread_data.frames.size() - 1];
			if (frame.lanes.size() == 0) {
				disconnect_on_error("Received a Pop with but no Push were received");
				return;
			}

			Lane &lane = frame.lanes.write[thread_data.current_lane_index];
			Item &last_item = lane.items.write[lane.items.size() - 1];
			CRASH_COND(last_item.begin_time == -1);
			last_item.end_time = event_time;

			if (thread_data.current_lane_index == -1) {
				disconnect_on_error("Received too many Pops");
				return;
			}
			--thread_data.current_lane_index;
		} break;

		case ZProfilingServer::EVENT_FRAME: {
			if (_last_received_thread_id == -1) {
				// TODO Maybe we could just ignore and wait until the next frame starts?
				disconnect_on_error("Thread ID not received yet (Frame)");
				return;
			}

			const uint32_t event_time = peer.get_u32();

			ThreadData &thread_data = _threads.write[_last_received_thread_id];

			if (thread_data.frames.size() > 0) {
				// Finish last frame
				Frame &last_frame = thread_data.frames.write[thread_data.frames.size() - 1];
				last_frame.end_time = event_time;

				if (thread_data.current_lane_index >= 0) {
					// TODO Eventually we could force-close scopes and reopen them on the next frame,
					// indicating that they "connect" with the previous frame?
					disconnect_on_error("Frame was marked but scopes are still active");
					return;
				}
			}

			_selected_frame = thread_data.frames.size() - 1;
			_flame_view->set_frame(_selected_frame);

			// Start new frame
			Frame new_frame;
			new_frame.begin_time = event_time;
			thread_data.frames.push_back(new_frame);
		} break;

		case ZProfilingServer::EVENT_STRING_DEF: {
			const uint16_t string_id = peer.get_u16();
			const String str = peer.get_utf8_string();

			String *existing_ptr = _strings.getptr(string_id);
			if (existing_ptr != nullptr) {
				print_line(String("WARNING: already received string {0}: `{1}`, newly received is `{2}`")
								   .format(varray(string_id, *existing_ptr, str)));
				*existing_ptr = str;
			} else {
				_strings.set(string_id, str);
			}
		} break;

		case ZProfilingServer::EVENT_THREAD: {
			const uint16_t thread_id = peer.get_u16();
			if (!_strings.has(thread_id)) {
				disconnect_on_error(String("Received Thread event with non-registered string {0}").format(varray(thread_id)));
				return;
			}
			if (thread_id >= _threads.size()) {
				_threads.resize(thread_id + 1);
			}
			_last_received_thread_id = thread_id;

			if (_selected_thread == -1) {
				_selected_thread = _last_received_thread_id;
				_flame_view->set_thread(_selected_thread);
			}
		} break;

		default:
			disconnect_on_error(String("Received unknown event {0}").format(varray(event_type)));
			return;
	}
}

int ZProfilingClient::get_thread_count() const {
	return _threads.size();
}

const ZProfilingClient::ThreadData &ZProfilingClient::get_thread_data(int thread_id) const {
	return _threads[thread_id];
}

const String ZProfilingClient::get_string(uint16_t str_id) const {
	const String *ptr = _strings.getptr(str_id);
	ERR_FAIL_COND_V(ptr == nullptr, String());
	return *ptr;
}

void ZProfilingClient::connect_to_host() {
	_peer->connect_to_host(IP_Address(ZProfilingServer::DEFAULT_HOST_ADDRESS), ZProfilingServer::DEFAULT_HOST_PORT);
	_connect_button->set_disabled(true);
}

void ZProfilingClient::disconnect_from_host() {
	_peer->disconnect_from_host();
	reset_connect_button();
}

void ZProfilingClient::_on_connect_button_pressed() {
	if (_peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		disconnect_from_host();
	} else {
		connect_to_host();
	}
}

void ZProfilingClient::disconnect_on_error(String message) {
	print_error(String("ERROR: ") + message);
	disconnect_from_host();
}

void ZProfilingClient::reset_connect_button() {
	_connect_button->set_disabled(false);
	_connect_button->set_text(TTR("Connect"));
}

void ZProfilingClient::_bind_methods() {
	// Internal
	ClassDB::bind_method(D_METHOD("_on_connect_button_pressed"), &ZProfilingClient::_on_connect_button_pressed);

	ClassDB::bind_method(D_METHOD("connect_to_host"), &ZProfilingClient::connect_to_host);
	ClassDB::bind_method(D_METHOD("disconnect_from_host"), &ZProfilingClient::disconnect_from_host);
}
