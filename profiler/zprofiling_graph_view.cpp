#include "zprofiling_graph_view.h"
#include "zprofiler.h"
#include "zprofiling_client.h"

ZProfilingGraphView::ZProfilingGraphView() {
}

void ZProfilingGraphView::set_client(ZProfilingClient *client) {
	_client = client;
}

void ZProfilingGraphView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW:
			_draw();
			break;

		default:
			break;
	}
}

static bool try_get_last_completed_frame_index(const ZProfilingClient::ThreadData &thread_data, int &out_frame_index) {
	if (thread_data.frames.size() == 0) {
		return false;
	}
	int i = thread_data.frames.size() - 1;
	while (i >= 0 && thread_data.frames[i].end_time == -1) {
		--i;
	}
	if (i < 0) {
		return false;
	}
	out_frame_index = i;
	return true;
}

// TODO Bake this once instead of computing it every time
static int get_frame_time(const ZProfilingClient::Frame &frame) {
	if (frame.lanes.size() == 0) {
		return 0;
	}
	int sum = 0;
	const ZProfilingClient::Lane &lane = frame.lanes[0];
	for (int i = 0; i < lane.items.size(); ++i) {
		const ZProfilingClient::Item &item = lane.items[i];
		sum += item.end_time - item.begin_time;
	}
	return sum;
}

void ZProfilingGraphView::_draw() {
	VOXEL_PROFILE_SCOPE();

	const Color bg_color(0.f, 0.f, 0.f, 0.7f);
	const Color curve_color(1.f, 0.5f, 0.f);
	const Color text_fg_color(1.f, 1.f, 1.f);
	const Color text_bg_color(0.f, 0.f, 0.f);
	const Color frame_graduation_color(1.f, 1.f, 1.f, 0.3f);

	// Background
	const Rect2 control_rect = get_rect();
	draw_rect(Rect2(0, 0, control_rect.size.x, control_rect.size.y), bg_color);

	ERR_FAIL_COND(_client == nullptr);
	int thread_index = _client->get_selected_thread();
	if (thread_index == -1) {
		return;
	}

	const ZProfilingClient::ThreadData &thread_data = _client->get_thread_data(thread_index);

	int max_frame;
	if (!try_get_last_completed_frame_index(thread_data, max_frame)) {
		return;
	}

	const int frame_width = 4;
	const int max_visible_frame_count = control_rect.size.x / frame_width;
	int min_frame = max_frame - max_visible_frame_count;
	if (min_frame < 0) {
		min_frame = 0;
	}

	const int min_x = control_rect.size.x - frame_width * (max_frame - min_frame);

	// Get normalized factor
	int max_frame_time = 1; // microseconds
	for (int frame_index = min_frame; frame_index < max_frame; ++frame_index) {
		const ZProfilingClient::Frame &frame = thread_data.frames[frame_index];
		const int frame_time = get_frame_time(frame);
		if (frame_time > max_frame_time) {
			max_frame_time = frame_time;
		}
	}

	// Add some headroom
	//max_frame_time = 10 * max_frame_time / 9;

	// Curve
	Rect2 item_rect(min_x, 0, frame_width, 0);
	for (int frame_index = min_frame; frame_index < max_frame; ++frame_index) {
		const ZProfilingClient::Frame &frame = thread_data.frames[frame_index];
		const int frame_time = get_frame_time(frame);
		item_rect.size.y = control_rect.size.y * (static_cast<float>(frame_time) / max_frame_time);
		item_rect.position.y = control_rect.size.y - item_rect.size.y;
		draw_rect(item_rect, curve_color);
		item_rect.position.x += frame_width;
	}

	Ref<Font> font = get_font("font");
	ERR_FAIL_COND(font.is_null());

	// Graduations

	// 16 ms limit
	int frame_graduation_y = control_rect.size.y - control_rect.size.y * (static_cast<float>(16000) / max_frame_time);
	draw_line(Vector2(0, frame_graduation_y), Vector2(control_rect.size.x, frame_graduation_y), frame_graduation_color);

	// Max ms
	const float max_frame_time_ms = static_cast<float>(max_frame_time) / 1000.f;
	String max_time_text = String("{0} ms").format(varray(max_frame_time_ms));
	draw_string(font, Vector2(5, 5 + font->get_ascent()), max_time_text, text_bg_color);
	draw_string(font, Vector2(4, 4 + font->get_ascent()), max_time_text, text_fg_color);
}

void ZProfilingGraphView::_bind_methods() {
}
