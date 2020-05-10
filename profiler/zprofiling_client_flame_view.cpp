#include "zprofiling_client_flame_view.h"
#include "zprofiler.h"
#include "zprofiling_client.h"

ZProfilingClientFlameView::ZProfilingClientFlameView() {
}

void ZProfilingClientFlameView::set_client(const ZProfilingClient *client) {
	_client = client;
}

void ZProfilingClientFlameView::set_frame(int frame_index) {
	if (frame_index != _frame_index) {
		_frame_index = frame_index;
		update();
	}
}

void ZProfilingClientFlameView::set_thread(int thread_id) {
	if (thread_id != _thread_id) {
		_thread_id = thread_id;
		update();
	}
}

void ZProfilingClientFlameView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW:
			_draw();
			break;

		default:
			break;
	}
}

void ZProfilingClientFlameView::_gui_input(Ref<InputEvent> p_event) {
}

void ZProfilingClientFlameView::_draw() {
	VOXEL_PROFILE_SCOPE(profile_scope);

	ERR_FAIL_COND(_client == nullptr);
	if (_thread_id >= _client->get_thread_count()) {
		return;
	}
	const ZProfilingClient::ThreadData &thread_data = _client->get_thread_data(_thread_id);

	// Pick last completed frame
	int frame_index = _frame_index;
	while (frame_index >= 0 && thread_data.frames[frame_index].end_time == -1) {
		--frame_index;
	}
	if (frame_index < 0) {
		return;
	}
	const ZProfilingClient::Frame &frame = thread_data.frames[frame_index];

	const Color item_color(1.f, 0.5f, 0.f);
	const Color bg_color(0.f, 0.f, 0.f, 0.7f);
	const int lane_separation = 1;
	const int lane_height = 32;

	// Background
	const Rect2 control_rect = get_rect();
	draw_rect(Rect2(0, 0, control_rect.size.x, control_rect.size.y), bg_color);

	Rect2 item_rect;
	item_rect.position.y = lane_separation;
	item_rect.size.y = lane_height;

	const int frame_duration = frame.end_time - frame.begin_time;
	const float frame_duration_inv = 1.f / frame_duration;

	// Lanes and items
	for (int lane_index = 0; lane_index < frame.lanes.size(); ++lane_index) {
		const ZProfilingClient::Lane &lane = frame.lanes[lane_index];

		for (int item_index = 0; item_index < lane.items.size(); ++item_index) {
			const ZProfilingClient::Item &item = lane.items[item_index];

			// TODO Cull items
			// TODO Pan and zoom

			item_rect.position.x = control_rect.size.x * static_cast<float>(item.begin_time - frame.begin_time) * frame_duration_inv;
			item_rect.size.x = control_rect.size.x * static_cast<float>(item.end_time - item.begin_time) * frame_duration_inv;

			if (item_rect.size.x < 1.01f) {
				// Clamp item size so it doesn't disappear
				item_rect.size.x = 1.01f;

			} else if (item_rect.size.x > 1.f) {
				// Give a small gap to see glued items distinctly
				item_rect.size.x -= 1.f;
			}

			draw_rect(item_rect, item_color);

			// TODO Draw item text
		}

		item_rect.position.y += lane_height + lane_separation;
	}

	// TODO Draw time graduations
}

void ZProfilingClientFlameView::_bind_methods() {
	ClassDB::bind_method("_gui_input", &ZProfilingClientFlameView::_gui_input);
}
