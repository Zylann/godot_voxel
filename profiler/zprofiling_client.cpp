#include "zprofiling_client.h"
#include "zprofiling_flame_view.h"
#include "zprofiling_graph_view.h"
#include "zprofiling_server.h"

#include <core/io/stream_peer_tcp.h>
#include <scene/gui/box_container.h>
#include <scene/gui/button.h>
#include <scene/gui/label.h>
#include <scene/gui/option_button.h>
#include <scene/gui/spin_box.h>
#include <scene/gui/split_container.h>

const uint32_t MAX_LANES = 256;
const uint32_t MAX_STRING_SIZE = 1000;
const int MAX_TIME_READING_EVENTS_MSEC = 32;

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

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_hb->add_child(spacer);

	Label *frame_label = memnew(Label);
	frame_label->set_text(TTR("Frame: "));
	header_hb->add_child(frame_label);

	_frame_spinbox = memnew(SpinBox);
	_frame_spinbox->set_min(0);
	_frame_spinbox->set_step(1);
	_frame_spinbox->connect("value_changed", this, "_on_frame_spinbox_value_changed");
	header_hb->add_child(_frame_spinbox);

	Label *thread_label = memnew(Label);
	thread_label->set_text(TTR("Thread: "));
	header_hb->add_child(thread_label);

	_thread_selector = memnew(OptionButton);
	_thread_selector->connect("item_selected", this, "_on_thread_selector_item_selected");
	header_hb->add_child(_thread_selector);

	_v_split_container = memnew(VSplitContainer);
	_v_split_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_v_split_container->set_split_offset(30);
	main_vb->add_child(_v_split_container);

	_graph_view = memnew(ZProfilingGraphView);
	_graph_view->set_client(this);
	//_graph_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_v_split_container->add_child(_graph_view);

	_flame_view = memnew(ZProfilingFlameView);
	_flame_view->set_client(this);
	//_flame_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_v_split_container->add_child(_flame_view);

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
	VOXEL_PROFILE_SCOPE();

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
		const int time_before = OS::get_singleton()->get_ticks_msec();
		int time_spent = 0;
		while (process_incoming_data() && time_spent < MAX_TIME_READING_EVENTS_MSEC) {
			time_spent = OS::get_singleton()->get_ticks_msec() - time_before;
		}
		if (time_spent >= MAX_TIME_READING_EVENTS_MSEC) {
			// This means we hit the bottleneck
			print_line("Event processing timed out");
		}
	}
}

bool ZProfilingClient::process_incoming_data() {
	CRASH_COND(_peer.is_null());
	StreamPeerTCP &peer = **_peer;

	const int available_bytes = peer.get_available_bytes();

	switch (_last_received_event_type) {
		case -1:
			if (available_bytes < 1) {
				return false;
			}
			_last_received_event_type = peer.get_u8();
			return true;

		case ZProfilingServer::EVENT_PUSH: {
			if (available_bytes < 6) {
				return false;
			}
			const uint16_t description_id = peer.get_u16();
			const uint32_t time = peer.get_u32();
			_last_received_event_type = -1;
			return process_event_push(time, description_id);
		}

		case ZProfilingServer::EVENT_POP: {
			if (available_bytes < 4) {
				return false;
			}
			const uint32_t time = peer.get_u32();
			_last_received_event_type = -1;
			return process_event_pop(time);
		}

		case ZProfilingServer::EVENT_FRAME: {
			if (available_bytes < 4) {
				return false;
			}
			const uint32_t time = peer.get_u32();
			_last_received_event_type = -1;
			return process_event_frame(time);
		}

		case ZProfilingServer::EVENT_THREAD: {
			if (available_bytes < 2) {
				return false;
			}
			const uint32_t thread_id = peer.get_u16();
			_last_received_event_type = -1;
			return process_event_thread(thread_id);
		}

		case ZProfilingServer::EVENT_STRING_DEF: {
			// Two-step process, strings have variable size

			if (_last_received_string_size == -1) {
				if (available_bytes < 6) {
					return false;
				}
				// First, get string header
				_last_received_string_id = peer.get_u16();
				_last_received_string_size = peer.get_u32();
				if (_last_received_string_size == 0) {
					disconnect_on_error("Received string size is zero");
					return false;
				}
				if (_last_received_string_size > MAX_STRING_SIZE) {
					disconnect_on_error(String("Received string size is too big: {0}").format(varray(_last_received_string_size)));
					return false;
				}
				return true;

			} else if (available_bytes >= _last_received_string_size) {
				// Get string itself
				Vector<uint8_t> data;
				data.resize(_last_received_string_size);
				const Error err = peer.get_data(data.ptrw(), data.size());
				if (err != OK) {
					disconnect_on_error(String("problem when getting string buffer: {0}").format(varray(err)));
					return false;
				}
				String str;
				if (str.parse_utf8((const char *)data.ptr(), data.size())) {
					disconnect_on_error("Error when parsing UTF8");
					return false;
				}
				const uint16_t string_id = _last_received_string_id;
				_last_received_event_type = -1;
				_last_received_string_id = -1;
				_last_received_string_size = -1;
				return process_event_string_def(string_id, str);
			}
		}

		default:
			disconnect_on_error(String("Received unknown event {0}").format(varray(_last_received_event_type)));
			break;
	}

	return false;
}

bool ZProfilingClient::process_event_push(uint32_t event_time, uint16_t description_id) {
	// Entered a profiling scope

	if (_last_received_thread_index == -1) {
		// TODO Maybe we could just ignore and wait until the next frame starts?
		disconnect_on_error("Thread ID not received yet (Push)");
		return false;
	}

	const String *string_ptr = _strings.getptr(description_id);
	if (string_ptr == nullptr) {
		disconnect_on_error(String("Push string {0} was not registered").format(varray(description_id)));
		return false;
	}

	ThreadData &thread_data = _threads.write[_last_received_thread_index];
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
		return false;
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
			return false;
		}
	}

	Item item;
	item.begin_time = event_time;
	item.description_id = description_id;
	lane.items.push_back(item);
	return true;
}

bool ZProfilingClient::process_event_pop(uint32_t event_time) {
	// Exited a profiling scope

	if (_last_received_thread_index == -1) {
		// TODO Maybe we could just ignore and wait until the next frame starts?
		disconnect_on_error("Thread ID not received yet (Pop)");
		return false;
	}

	ThreadData &thread_data = _threads.write[_last_received_thread_index];
	Frame &frame = thread_data.frames.write[thread_data.frames.size() - 1];
	if (frame.lanes.size() == 0) {
		disconnect_on_error("Received a Pop with but no Push were received");
		return false;
	}

	Lane &lane = frame.lanes.write[thread_data.current_lane_index];
	Item &last_item = lane.items.write[lane.items.size() - 1];
	CRASH_COND(last_item.begin_time == -1);
	last_item.end_time = event_time;

	if (thread_data.current_lane_index == -1) {
		disconnect_on_error("Received too many Pops");
		return false;
	}
	--thread_data.current_lane_index;
	return true;
}

bool ZProfilingClient::process_event_frame(uint32_t event_time) {
	// Frame started (or ended)

	if (_last_received_thread_index == -1) {
		// TODO Maybe we could just ignore and wait until the next frame starts?
		disconnect_on_error("Thread ID not received yet (Frame)");
		return false;
	}

	ThreadData &thread_data = _threads.write[_last_received_thread_index];

	if (thread_data.frames.size() > 0) {
		// Finish last frame
		Frame &last_frame = thread_data.frames.write[thread_data.frames.size() - 1];
		last_frame.end_time = event_time;

		if (thread_data.current_lane_index >= 0) {
			// TODO Eventually we could force-close scopes and reopen them on the next frame,
			// indicating that they "connect" with the previous frame?
			disconnect_on_error("Frame was marked but scopes are still active");
			return false;
		}
	}

	//print_line(String("Frame {0} of thread {1}").format(varray(thread_data.frames.size(), thread_id)));

	// Start new frame
	Frame new_frame;
	new_frame.begin_time = event_time;
	thread_data.frames.push_back(new_frame);

	_frame_spinbox->set_max(thread_data.frames.size() - 1);

	_graph_view->update();

	// TODO Only do this if the user wants to keep update to last frame
	if (_selected_thread_index == _last_received_thread_index) {
		set_selected_frame(thread_data.frames.size() - 1);
	}
	return true;
}

bool ZProfilingClient::process_event_string_def(uint16_t string_id, String str) {
	// New string definition

	String *existing_ptr = _strings.getptr(string_id);
	if (existing_ptr != nullptr) {
		print_line(String("WARNING: already received string {0}: previous was `{1}`, newly received is `{2}`")
						   .format(varray(string_id, *existing_ptr, str)));
		*existing_ptr = str;
	} else {
		print_line(String("Registering string {0}: {1}").format(varray(string_id, str)));
		_strings.set(string_id, str);
	}
	return true;
}

bool ZProfilingClient::process_event_thread(uint16_t thread_id) {
	// From now on, next events will be about this thread

	if (!_strings.has(thread_id)) {
		disconnect_on_error(String("Received Thread event with non-registered string {0}").format(varray(thread_id)));
		return false;
	}

	const int existing_thread_index = get_thread_index_from_id(thread_id);

	if (existing_thread_index == -1) {
		ThreadData thread_data;
		thread_data.id = thread_id;
		_last_received_thread_index = _threads.size();
		_threads.push_back(thread_data);
		update_thread_list();

	} else {
		_last_received_thread_index = existing_thread_index;
	}

	if (_selected_thread_index == -1) {
		_selected_thread_index = _last_received_thread_index;
		_flame_view->set_thread(_selected_thread_index);
	}

	return true;
}

int ZProfilingClient::get_thread_index_from_id(uint16_t thread_id) const {
	for (int i = 0; i < _threads.size(); ++i) {
		const ThreadData &tdata = _threads[i];
		if (tdata.id == thread_id) {
			return i;
		}
	}
	return -1;
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
	clear_profiling_data();

	_peer->connect_to_host(IP_Address(ZProfilingServer::DEFAULT_HOST_ADDRESS), ZProfilingServer::DEFAULT_HOST_PORT);
	_connect_button->set_disabled(true);
}

void ZProfilingClient::disconnect_from_host() {
	_peer->disconnect_from_host();
	reset_connect_button();
	clear_network_states();
}

void ZProfilingClient::clear_network_states() {
	_previous_peer_status = -1;
	_last_received_event_type = -1;
	_last_received_string_id = -1;
	_last_received_string_size = -1;
	_last_received_thread_index = -1;
}

void ZProfilingClient::clear_profiling_data() {
	_strings.clear();
	_threads.clear();
	_selected_thread_index = -1;

	update_thread_list();
	_flame_view->update();

	_frame_spinbox_ignore_changes = true;
	_frame_spinbox->set_value(0);
	_frame_spinbox->set_max(0);
	_frame_spinbox_ignore_changes = false;
}

void ZProfilingClient::_on_connect_button_pressed() {
	if (_peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		disconnect_from_host();
	} else {
		connect_to_host();
	}
}

void ZProfilingClient::_on_frame_spinbox_value_changed(float value) {
	if (_frame_spinbox_ignore_changes) {
		return;
	}
	set_selected_frame((int)value);
}

void ZProfilingClient::_on_thread_selector_item_selected(int idx) {
	set_selected_thread(idx);
}

void ZProfilingClient::disconnect_on_error(String message) {
	ERR_PRINT(String("ERROR: ") + message);
	disconnect_from_host();
}

void ZProfilingClient::reset_connect_button() {
	_connect_button->set_disabled(false);
	_connect_button->set_text(TTR("Connect"));
}

void ZProfilingClient::set_selected_thread(int thread_index) {
	if (thread_index == _selected_thread_index) {
		return;
	}

	ERR_FAIL_COND(thread_index >= _threads.size());
	_selected_thread_index = thread_index;
	_flame_view->set_thread(_selected_thread_index);

	_graph_view->update();

	print_line(String("Selected thread {0}").format(varray(_selected_thread_index)));
	_thread_selector->select(thread_index);

	const ThreadData &thread_data = _threads[_selected_thread_index];
	_frame_spinbox_ignore_changes = true;
	_frame_spinbox->set_max(thread_data.frames.size() - 1);
	_frame_spinbox_ignore_changes = false;

	if (thread_data.frames.size() > 0) {
		set_selected_frame(thread_data.frames.size() - 1);
	}
}

int ZProfilingClient::get_selected_thread() const {
	return _selected_thread_index;
}

void ZProfilingClient::set_selected_frame(int frame_index) {
	ERR_FAIL_COND(_selected_thread_index == -1);
	ThreadData &thread_data = _threads.write[_selected_thread_index];

	ERR_FAIL_COND(thread_data.frames.size() == 0);
	ERR_FAIL_COND(frame_index >= thread_data.frames.size());

	// Clamp to last completed frame
	while (frame_index >= 0 && thread_data.frames[frame_index].end_time == -1) {
		--frame_index;
	}
	if (frame_index < 0) {
		// No complete frame available yet
		// TODO Support viewing partial frames?
		return;
	}

	if (thread_data.selected_frame == frame_index) {
		return;
	}

	thread_data.selected_frame = frame_index;
	_flame_view->update();
	_graph_view->update();

	// This can emit again and cycle back to our method...
	// hence why checking if it changed is important
	_frame_spinbox->set_value(thread_data.selected_frame);
}

void ZProfilingClient::update_thread_list() {
	for (size_t i = 0; i < _threads.size(); ++i) {
		const ThreadData &thread_data = _threads[i];
		const String *name_ptr = _strings.getptr(thread_data.id);
		ERR_FAIL_COND(name_ptr == nullptr);

		if (i < _thread_selector->get_item_count()) {
			_thread_selector->set_item_text(i, *name_ptr);
		} else {
			_thread_selector->add_item(*name_ptr);
		}
	}

	while (_thread_selector->get_item_count() > _threads.size()) {
		_thread_selector->remove_item(_thread_selector->get_item_count() - 1);
	}
}

void ZProfilingClient::_bind_methods() {
	// Internal
	ClassDB::bind_method(D_METHOD("_on_connect_button_pressed"), &ZProfilingClient::_on_connect_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_frame_spinbox_value_changed"), &ZProfilingClient::_on_frame_spinbox_value_changed);
	ClassDB::bind_method(D_METHOD("_on_thread_selector_item_selected"), &ZProfilingClient::_on_thread_selector_item_selected);

	ClassDB::bind_method(D_METHOD("connect_to_host"), &ZProfilingClient::connect_to_host);
	ClassDB::bind_method(D_METHOD("disconnect_from_host"), &ZProfilingClient::disconnect_from_host);
}
