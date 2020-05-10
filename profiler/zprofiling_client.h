#ifndef ZPROFILING_CLIENT_H
#define ZPROFILING_CLIENT_H

#include <core/hash_map.h>
#include <scene/gui/control.h>

class StreamPeerTCP;
class Button;
class Label;
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
		Vector<Frame> frames;
		int current_lane_index = -1;
	};

	ZProfilingClient();
	~ZProfilingClient();

	void connect_to_host();
	void disconnect_from_host();
	int get_thread_count() const;
	const ThreadData &get_thread_data(int thread_id) const;
	const String get_string(uint16_t str_id) const;

private:
	void _notification(int p_what);
	void _process();
	void _process_connected();
	void _on_connect_button_pressed();

	void reset_connect_button();
	void disconnect_on_error(String message);

	static void _bind_methods();

	// GUI
	Button *_connect_button = nullptr;
	Label *_status_label = nullptr;
	ZProfilingClientFlameView *_flame_view = nullptr;

	// Data
	Vector<ThreadData> _threads;
	HashMap<uint16_t, String> _strings;
	int _selected_thread = -1;
	int _selected_frame = -1;
	bool _follow_last_frame = true;

	// Network
	Ref<StreamPeerTCP> _peer;
	int _previous_peer_status = -1;
	int _last_received_thread_id = -1;
};

#endif // ZPROFILING_CLIENT_H
