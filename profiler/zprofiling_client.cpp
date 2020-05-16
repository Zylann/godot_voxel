#include "zprofiling_client.h"
#include "zprofiling_graph_view.h"
#include "zprofiling_server.h"
#include "zprofiling_timeline_view.h"
#include "zprofiling_tree_view.h"

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

	HSplitContainer *h_split_container = memnew(HSplitContainer);
	h_split_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	h_split_container->set_split_offset(100);
	main_vb->add_child(h_split_container);

	_tree_view = memnew(ZProfilingTreeView);
	_tree_view->set_client(this);
	h_split_container->add_child(_tree_view);

	VSplitContainer *v_split_container = memnew(VSplitContainer);
	v_split_container->set_split_offset(30);
	h_split_container->add_child(v_split_container);

	_graph_view = memnew(ZProfilingGraphView);
	_graph_view->set_client(this);
	_graph_view->connect(ZProfilingGraphView::SIGNAL_FRAME_CLICKED, this, "_on_graph_view_frame_clicked");
	_graph_view->connect(ZProfilingGraphView::SIGNAL_MOUSE_WHEEL_MOVED, this, "_on_graph_view_mouse_wheel_moved");
	v_split_container->add_child(_graph_view);

	_timeline_view = memnew(ZProfilingTimelineView);
	_timeline_view->set_client(this);
	v_split_container->add_child(_timeline_view);

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
	ZPROFILER_SCOPE_NAMED(FUNCTION_STR);

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
				_auto_select_main = true;
				break;

			case StreamPeerTCP::STATUS_NONE:
				_status_label->set_text(TTR("Status: disconnected"));
				reset_connect_button();
				_auto_select_main = true;
				break;

			default:
				CRASH_NOW_MSG("Unhandled status");
				break;
		}
	}

	if (peer_status == StreamPeerTCP::STATUS_CONNECTED) {
		uint64_t time_before = OS::get_singleton()->get_ticks_msec();
		while (process_incoming_data()) {
			if (OS::get_singleton()->get_ticks_msec() - time_before > MAX_TIME_READING_EVENTS_MSEC) {
				print_line("Spent too long processing incoming data");
				break;
			}
		}
		//print_line(String("Remaining data: {0}").format(varray(_peer->get_available_bytes())));
	}
}

// Returns true as long as it should be called again
bool ZProfilingClient::process_incoming_data() {
	ZPROFILER_SCOPE_NAMED(FUNCTION_STR);
	CRASH_COND(_peer.is_null());
	StreamPeerTCP &peer = **_peer;

	if (_last_received_block_size == -1) {
		// Waiting for next block header
		const int available_bytes = peer.get_available_bytes();

		if (available_bytes < 5) {
			// Data not arrived yet
			return false;
		} else {
			const uint8_t event_type = peer.get_u8();
			if (event_type != ZProfilingServer::EVENT_INCOMING_DATA_SIZE) {
				disconnect_on_error(String("Expected incoming data block header, got {0}")
											.format(varray(event_type)));
				return false;
			}
			_last_received_block_size = peer.get_u32();
			return true;
		}
	}

	if (_received_data.size() < _last_received_block_size) {
		// Block is incomplete

		const int remaining_bytes = _last_received_block_size - _received_data.size();
		const int available_bytes = peer.get_available_bytes();
		const int available_bytes_for_block = MIN(available_bytes, remaining_bytes);

		if (available_bytes_for_block == 0) {
			// Data not arrived yet
			return false;
		} else {
			size_t start_index = _received_data.size();
			_received_data.resize(_received_data.size() + available_bytes_for_block);
			const Error err = peer.get_data(_received_data.data() + start_index, available_bytes_for_block);
			if (err != OK) {
				disconnect_on_error(String("Could not get data size {0} for block, error {1}")
											.format(varray(_last_received_block_size, err)));
				return false;
			}
			// We should continue only when the received data is complete
			return _received_data.size() == _last_received_block_size;
		}
	}

	// We have a complete block of data, read it
	while (!_received_data.is_end()) {
		const uint8_t event_type = _received_data.get_u8();

		switch (event_type) {
			case ZProfilingServer::EVENT_FRAME: {
				const uint16_t thread_name_id = _received_data.get_u16();
				const uint64_t frame_end_time = _received_data.get_u64();

				if (!_strings.has(thread_name_id)) {
					disconnect_on_error(String("Received Thread event with non-registered string {0}")
												.format(varray(thread_name_id)));
					return false;
				}

				// Get thread
				const int existing_thread_index = get_thread_index_from_id(thread_name_id);
				int thread_index = -1;

				if (existing_thread_index == -1) {
					ThreadData thread_data;
					thread_data.id = thread_name_id;
					thread_index = _threads.size();
					_threads.push_back(thread_data);
					update_thread_list();

					if (_auto_select_main) {
						_auto_select_main = !try_auto_select_main_thread();
					}

				} else {
					thread_index = existing_thread_index;
				}

				ThreadData &thread_data = _threads.write[thread_index];

				if (thread_data.frames.size() == 0) {
					// Initial frame, normally with no events inside,
					// as frame markers must be at the beginning
					thread_data.frames.push_back(Frame());
					Frame &initial_frame = thread_data.frames.write[0];
					initial_frame.end_time = frame_end_time;
				}

				_frame_spinbox->set_max(thread_data.frames.size() - 1);
				_graph_view->update();

				// Finalize frame
				Frame &frame = thread_data.frames.write[thread_data.frames.size() - 1];
				frame.end_time = frame_end_time;

				for (size_t i = 0; i < ZProfilingServer::MAX_LANES; ++i) {
					const uint32_t item_count = _received_data.get_u32();
					if (item_count == 0) {
						break;
					}
					Lane lane;
					lane.items.resize(item_count);
					_received_data.get_data((uint8_t *)lane.items.ptrw(), item_count * sizeof(Item));
					frame.lanes.push_back(lane);
				}

				// Start next frame
				Frame next_frame;
				next_frame.begin_time = frame_end_time;
				thread_data.frames.push_back(next_frame);
			} break;

			case ZProfilingServer::EVENT_STRING_DEF: {
				const uint16_t string_id = _received_data.get_u16();
				const uint16_t string_size = _received_data.get_u32();

				Vector<uint8_t> data;
				data.resize(string_size);
				_received_data.get_data(data.ptrw(), data.size());

				String str;
				if (str.parse_utf8((const char *)data.ptr(), data.size())) {
					disconnect_on_error("Error when parsing UTF8");
					return false;
				}

				if (!process_event_string_def(string_id, str)) {
					return false;
				}
			} break;

			case ZProfilingServer::EVENT_INCOMING_DATA_SIZE:
				disconnect_on_error("Received unexpected incoming data size header");
				return false;
				break;

			default:
				disconnect_on_error(String("Received unknown event {0}").format(varray(event_type)));
				break;
		}
	}

	// Done reading that block
	_received_data.clear();
	_last_received_block_size = -1;
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

const ZProfilingClient::Frame *ZProfilingClient::get_frame(int thread_index, int frame_index) const {
	if (thread_index >= get_thread_count()) {
		return nullptr;
	}
	const ZProfilingClient::ThreadData &thread_data = get_thread_data(thread_index);
	if (frame_index < 0 || frame_index >= thread_data.frames.size()) {
		return nullptr;
	}
	const ZProfilingClient::Frame &frame = thread_data.frames[frame_index];
	if (frame.end_time == -1) {
		// Frame is not finalized
		return nullptr;
	}
	return &frame;
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
	_auto_select_main = false;
	clear_network_states();
}

void ZProfilingClient::clear_network_states() {
	_previous_peer_status = -1;
	_last_received_block_size = -1;
	_received_data.clear();
}

void ZProfilingClient::clear_profiling_data() {
	_threads.clear();

	_strings.clear();
	_selected_thread_index = -1;

	update_thread_list();
	_timeline_view->update();

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

void ZProfilingClient::_on_graph_view_frame_clicked(int frame_index) {
	if (frame_index < 0) {
		return;
	}
	if (get_selected_thread() == -1) {
		return;
	}
	const ThreadData &thread_data = get_thread_data(get_selected_thread());
	if (frame_index >= thread_data.frames.size()) {
		return;
	}
	set_selected_frame(frame_index);
}

void ZProfilingClient::_on_graph_view_mouse_wheel_moved(int delta) {
	if (get_selected_thread() == -1) {
		return;
	}
	const ThreadData &thread_data = get_thread_data(get_selected_thread());
	int new_frame_index = thread_data.selected_frame + delta;
	if (new_frame_index < 0) {
		new_frame_index = 0;
	}
	if (new_frame_index >= thread_data.frames.size()) {
		new_frame_index = thread_data.frames.size() - 1;
	}
	set_selected_frame(new_frame_index);
}

void ZProfilingClient::disconnect_on_error(String message) {
	ERR_PRINT(String("ERROR: ") + message);
	disconnect_from_host();
}

void ZProfilingClient::reset_connect_button() {
	_connect_button->set_disabled(false);
	_connect_button->set_text(TTR("Connect"));
}

bool ZProfilingClient::try_auto_select_main_thread() {
	for (size_t i = 0; i < _threads.size(); ++i) {
		const ThreadData &t = _threads[i];
		String *name_ptr = _strings.getptr(t.id);
		CRASH_COND(name_ptr == nullptr);
		// TODO Have a more "official" way to say which thread is main?
		if (name_ptr->findn("main") != -1) {
			set_selected_thread(i);
			return true;
		}
	}
	return false;
}

void ZProfilingClient::set_selected_thread(int thread_index) {
	if (thread_index == _selected_thread_index) {
		return;
	}

	ERR_FAIL_COND(thread_index >= _threads.size());
	_selected_thread_index = thread_index;
	_timeline_view->set_thread(_selected_thread_index);
	_tree_view->set_thread_index(_selected_thread_index);

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
	_timeline_view->set_frame_index(frame_index);
	_tree_view->set_frame_index(frame_index);
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
	ClassDB::bind_method(D_METHOD("_on_graph_view_frame_clicked"), &ZProfilingClient::_on_graph_view_frame_clicked);
	ClassDB::bind_method(D_METHOD("_on_graph_view_mouse_wheel_moved"), &ZProfilingClient::_on_graph_view_mouse_wheel_moved);

	ClassDB::bind_method(D_METHOD("connect_to_host"), &ZProfilingClient::connect_to_host);
	ClassDB::bind_method(D_METHOD("disconnect_from_host"), &ZProfilingClient::disconnect_from_host);
}
