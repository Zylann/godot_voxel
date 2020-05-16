#ifndef ZPROFILING_CLIENT_H
#define ZPROFILING_CLIENT_H

#include "zprofiling_receive_buffer.h"
#include <core/hash_map.h>
#include <scene/gui/control.h>

class StreamPeerTCP;
class Button;
class Label;
class SpinBox;
class OptionButton;
class VSplitContainer;
class ZProfilingTimelineView;
class ZProfilingGraphView;
class ZProfilingTreeView;

class ZProfilingClient : public Control {
	GDCLASS(ZProfilingClient, Control)
public:
	struct Item {
		// This layout must match what the server sends
		uint32_t begin_time_relative = 0;
		uint32_t end_time_relative = 0;
		uint16_t description_id = 0;
	};

	struct Lane {
		Vector<Item> items;
	};

	struct Frame {
		Vector<Lane> lanes;
		uint64_t begin_time = 0;
		uint64_t end_time = 0;
	};

	struct ThreadData {
		uint16_t id = 0;
		// TODO Drop/dump frames that go beyond a fixed time
		Vector<Frame> frames;
		int selected_frame = -1;
	};

	ZProfilingClient();
	~ZProfilingClient();

	void connect_to_host();
	void disconnect_from_host();
	int get_thread_count() const;
	const ThreadData &get_thread_data(int thread_id) const;
	const ZProfilingClient::Frame *get_frame(int thread_index, int frame_index) const;
	const String get_string(uint16_t str_id) const;
	void set_selected_frame(int frame_index);
	void set_selected_thread(int thread_index);
	int get_selected_thread() const;

private:
	void _notification(int p_what);
	void _process();

	bool process_incoming_data();
	bool process_event_string_def(uint16_t string_id, String str);

	void _on_connect_button_pressed();
	void _on_frame_spinbox_value_changed(float value);
	void _on_thread_selector_item_selected(int idx);
	void _on_graph_view_frame_clicked(int frame_index);
	void _on_graph_view_mouse_wheel_moved(int delta);

	void reset_connect_button();
	bool try_auto_select_main_thread();
	void disconnect_on_error(String message);
	void update_thread_list();
	int get_thread_index_from_id(uint16_t thread_id) const;
	void clear_profiling_data();
	void clear_network_states();

	static void _bind_methods();

	// GUI
	Button *_connect_button = nullptr;
	Label *_status_label = nullptr;
	ZProfilingGraphView *_graph_view = nullptr;
	ZProfilingTimelineView *_timeline_view = nullptr;
	ZProfilingTreeView *_tree_view = nullptr;
	SpinBox *_frame_spinbox = nullptr;
	bool _frame_spinbox_ignore_changes = false;
	OptionButton *_thread_selector = nullptr;
	bool _auto_select_main = true;

	// Data
	Vector<ThreadData> _threads;
	HashMap<uint16_t, String> _strings;
	int _selected_thread_index = -1;
	bool _follow_last_frame = true;

	// Network
	Ref<StreamPeerTCP> _peer;
	ZProfilingReceiveBuffer _received_data;
	int _previous_peer_status = -1;
	int _last_received_block_size = -1;
};

#endif // ZPROFILING_CLIENT_H
