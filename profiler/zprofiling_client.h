#ifndef ZPROFILING_CLIENT_H
#define ZPROFILING_CLIENT_H

#include <core/hash_map.h>
#include <scene/gui/control.h>

class StreamPeerTCP;
class Button;
class Label;
class SpinBox;
class OptionButton;
class ZProfilingClientFlameView;

class ZProfilingClient : public Control {
	GDCLASS(ZProfilingClient, Control)
public:
	struct Item {
		int begin_time = -1;
		int end_time = -1;
		uint16_t description_id = 0;
	};

	struct Lane {
		Vector<Item> items;
	};

	struct Frame {
		Vector<Lane> lanes;
		int begin_time = -1;
		int end_time = -1;
	};

	struct ThreadData {
		uint16_t id = 0;
		// TODO Drop/dump frames that go beyond a fixed time
		Vector<Frame> frames;
		int current_lane_index = -1;
		int selected_frame = -1;
	};

	ZProfilingClient();
	~ZProfilingClient();

	void connect_to_host();
	void disconnect_from_host();
	int get_thread_count() const;
	const ThreadData &get_thread_data(int thread_id) const;
	const String get_string(uint16_t str_id) const;
	void set_selected_frame(int frame_index);
	void set_selected_thread(int thread_index);

private:
	void _notification(int p_what);
	void _process();

	bool process_incoming_data();
	bool process_event_push(uint32_t event_time, uint16_t description_id);
	bool process_event_pop(uint32_t event_time);
	bool process_event_frame(uint32_t event_time);
	bool process_event_string_def(uint16_t string_id, String str);
	bool process_event_thread(uint16_t thread_id);

	void _on_connect_button_pressed();
	void _on_frame_spinbox_value_changed(float value);
	void _on_thread_selector_item_selected(int idx);

	void reset_connect_button();
	void disconnect_on_error(String message);
	void update_thread_list();
	int get_thread_index_from_id(uint16_t thread_id) const;
	void clear_profiling_data();
	void clear_network_states();

	static void _bind_methods();

	// GUI
	Button *_connect_button = nullptr;
	Label *_status_label = nullptr;
	ZProfilingClientFlameView *_flame_view = nullptr;
	SpinBox *_frame_spinbox = nullptr;
	OptionButton *_thread_selector = nullptr;
	bool _frame_spinbox_ignore_changes = false;

	// Data
	Vector<ThreadData> _threads;
	HashMap<uint16_t, String> _strings;
	int _selected_thread_index = -1;
	bool _follow_last_frame = true;

	// Network
	Ref<StreamPeerTCP> _peer;
	int _previous_peer_status = -1;
	int _last_received_thread_index = -1;
	int _last_received_event_type = -1;
	int _last_received_string_size = -1;
	int _last_received_string_id = -1;
};

#endif // ZPROFILING_CLIENT_H
