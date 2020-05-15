#include "zprofiling_timeline_view.h"
#include "zprofiler.h"
#include "zprofiling_client.h"

const double ZOOM_FACTOR = 1.25;
const double MINIMUM_TIME_WIDTH_US = 1.0;

static const ZProfilingClient::Frame *get_frame(const ZProfilingClient *client, int thread_index, int frame_index) {
	ERR_FAIL_COND_V(client == nullptr, nullptr);
	if (thread_index >= client->get_thread_count()) {
		return nullptr;
	}
	const ZProfilingClient::ThreadData &thread_data = client->get_thread_data(thread_index);
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

ZProfilingTimelineView::ZProfilingTimelineView() {
	set_clip_contents(true);
}

void ZProfilingTimelineView::set_client(const ZProfilingClient *client) {
	_client = client;
}

void ZProfilingTimelineView::set_thread(int thread_index) {
	if (_thread_index != thread_index) {
		_thread_index = thread_index;
		const ZProfilingClient::Frame *frame = get_frame(_client, _thread_index, _frame_index);
		if (frame != nullptr) {
			set_view_range(0, frame->end_time - frame->begin_time);
		}
		update();
	}
}

void ZProfilingTimelineView::set_frame_index(int frame_index) {
	if (_frame_index != frame_index) {
		_frame_index = frame_index;
		const ZProfilingClient::Frame *frame = get_frame(_client, _thread_index, _frame_index);
		if (frame != nullptr) {
			set_view_range(0, frame->end_time - frame->begin_time);
		}
		update();
	}
}

void ZProfilingTimelineView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW:
			_draw();
			break;

		default:
			break;
	}
}

void ZProfilingTimelineView::_gui_input(Ref<InputEvent> p_event) {
	const ZProfilingClient::Frame *frame = get_frame(_client, _thread_index, _frame_index);
	if (frame == nullptr) {
		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			switch (mb->get_button_index()) {
				case BUTTON_WHEEL_UP:
					add_zoom(1.f / ZOOM_FACTOR, mb->get_position().x);
					break;

				case BUTTON_WHEEL_DOWN:
					add_zoom(ZOOM_FACTOR, mb->get_position().x);
					break;

				default:
					break;
			}
		}
	}

	if (mm.is_valid()) {
		if ((mm->get_button_mask() & BUTTON_MASK_MIDDLE) != 0) {
			const int time_width = _view_max_time_us - _view_min_time_us;
			const float microseconds_per_pixel = time_width / get_rect().size.x;
			double offset = -mm->get_relative().x * microseconds_per_pixel;

			const uint64_t frame_duration = frame->end_time - frame->begin_time;

			if (offset > 0) {
				if (_view_max_time_us + offset > frame_duration) {
					offset = frame_duration - _view_max_time_us;
				}
			} else {
				if (_view_min_time_us + offset < 0) {
					offset = -_view_min_time_us;
				}
			}

			set_view_range(_view_min_time_us + offset, _view_max_time_us + offset);
			update();
		}
	}
}

void ZProfilingTimelineView::add_zoom(float factor, float mouse_x) {
	double time_offset = _view_min_time_us;
	double time_width = _view_max_time_us - _view_min_time_us;

	const double d = mouse_x / get_rect().size.x;
	const double prev_width = time_width;
	time_width = time_width * factor;
	time_offset -= d * (time_width - prev_width);

	if (time_width < MINIMUM_TIME_WIDTH_US) {
		time_width = MINIMUM_TIME_WIDTH_US;
	}

	set_view_range(time_offset, time_offset + time_width);
}

void ZProfilingTimelineView::set_view_range(float min_time_us, float max_time_us) {
	const ZProfilingClient::Frame *frame = get_frame(_client, _thread_index, _frame_index);
	ERR_FAIL_COND(frame == nullptr);
	const uint64_t frame_duration_us = frame->end_time - frame->begin_time;

	// Clamp
	if (min_time_us < 0) {
		min_time_us = 0;
	}
	if (max_time_us > frame_duration_us) {
		max_time_us = frame_duration_us;
	}

	if (min_time_us == _view_min_time_us && max_time_us == _view_max_time_us) {
		return;
	}

	_view_min_time_us = min_time_us;
	_view_max_time_us = max_time_us;

	update();
}

static void draw_shaded_text(CanvasItem *ci, Ref<Font> font, Vector2 pos, String text, Color fg, Color bg) {
	ci->draw_string(font, pos + Vector2(1, 1), text, bg);
	ci->draw_string(font, pos, text, fg);
}

static String get_left_ellipsed_text(String text, int max_width, Ref<Font> font, Vector2 &out_string_size) {
	if (text.length() == 0) {
		out_string_size = Vector2(0, font->get_height());
		return text;
	}

	float w = 0;
	const String ellipsis = "...";
	const float ellipsis_width = font->get_string_size(ellipsis).x;

	// Iterate characters from right to left
	int i = text.size() - 1;
	for (; i >= 0; --i) {
		w += font->get_char_size(text[i], text[i + 1]).width;

		if (w > max_width) {
			break;
		}
	}

	if (i >= 0) {
		// Ellipsis is needed
		w += ellipsis_width;

		// Iterate forward until it fits with the ellipsis
		for (; i < text.size(); ++i) {
			w -= font->get_char_size(text[i], text[i + 1]).width;

			if (w <= max_width) {
				break;
			}
		}

		text = ellipsis + text.right(i);
	}

	out_string_size = Vector2(w, font->get_height());
	return text;
}

void ZProfilingTimelineView::_draw() {
	VOXEL_PROFILE_SCOPE();

	const Color item_color(1.f, 0.5f, 0.f);
	const Color bg_color(0.f, 0.f, 0.f, 0.7f);
	const Color item_text_color(0.f, 0.f, 0.f);
	const Color text_fg_color(1.f, 1.f, 1.f);
	const Color text_bg_color(0.f, 0.f, 0.f);
	const int lane_separation = 1;
	const int lane_height = 20;
	const Vector2 text_margin(4, 4);

	// Background
	const Rect2 control_rect = get_rect();
	draw_rect(Rect2(0, 0, control_rect.size.x, control_rect.size.y), bg_color);

	Ref<Font> font = get_font("font");
	ERR_FAIL_COND(font.is_null());

	const ZProfilingClient::Frame *frame = get_frame(_client, _thread_index, _frame_index);
	if (frame == nullptr) {
		return;
	}

	Rect2 item_rect;
	item_rect.position.y = lane_separation;
	item_rect.size.y = lane_height;

	const float frame_view_duration_us = _view_max_time_us - _view_min_time_us;
	const float frame_view_duration_us_inv = 1.f / frame_view_duration_us;

	// Lanes and items
	for (int lane_index = 0; lane_index < frame->lanes.size(); ++lane_index) {
		const ZProfilingClient::Lane &lane = frame->lanes[lane_index];

		for (int item_index = 0; item_index < lane.items.size(); ++item_index) {
			const ZProfilingClient::Item &item = lane.items[item_index];

			item_rect.position.x = control_rect.size.x *
								   (static_cast<double>(item.begin_time_relative) - _view_min_time_us) *
								   frame_view_duration_us_inv;

			item_rect.size.x = control_rect.size.x *
							   (static_cast<double>(item.end_time_relative) - item.begin_time_relative) *
							   frame_view_duration_us_inv;

			if (item_rect.position.x + item_rect.size.x < 0) {
				// Out of view
				continue;
			}
			if (item_rect.position.x > control_rect.size.x) {
				// Out of view, next items also will
				break;
			}

			if (item_rect.size.x < 1.01f) {
				// Clamp item size so it doesn't disappear
				item_rect.size.x = 1.01f;

			} else if (item_rect.size.x > 1.f) {
				// Give a small gap to see glued items distinctly
				item_rect.size.x -= 1.f;
			}

			draw_rect(item_rect, item_color);

			if (item_rect.size.x > 100) {
				String text = _client->get_string(item.description_id);

				int clamped_item_width = item_rect.size.x;
				if (item_rect.position.x < 0) {
					clamped_item_width += item_rect.position.x;
				}

				const int text_area_width = clamped_item_width - 2 * text_margin.x;
				Vector2 text_size;
				text = get_left_ellipsed_text(text, text_area_width, font, text_size);

				Vector2 text_pos(
						item_rect.position.x + text_margin.x,
						item_rect.position.y + text_margin.y + font->get_ascent());

				// If the item starts off-screen, clamp it to the left
				if (text_pos.x < text_margin.x) {
					text_pos.x = text_margin.x;
				}

				draw_string(font, text_pos, text, item_text_color, item_rect.size.x);
			}
		}

		item_rect.position.y += lane_height + lane_separation;
	}

	// Time graduations

	const float min_time_ms = _view_min_time_us / 1000.0;
	const float max_time_ms = _view_max_time_us / 1000.0;

	const String begin_time_text = String("{0} ms").format(varray(min_time_ms));
	const String end_time_text = String("{0} ms").format(varray(max_time_ms));

	const Vector2 min_text_pos(4, control_rect.size.y - font->get_height() + font->get_ascent());
	const Vector2 max_text_size = font->get_string_size(end_time_text);
	const Vector2 max_text_pos(control_rect.size.x - 4 - max_text_size.x, control_rect.size.y - font->get_height() + font->get_ascent());

	draw_shaded_text(this, font, min_text_pos, begin_time_text, text_fg_color, text_bg_color);
	draw_shaded_text(this, font, max_text_pos, end_time_text, text_fg_color, text_bg_color);
}

void ZProfilingTimelineView::_bind_methods() {
	ClassDB::bind_method("_gui_input", &ZProfilingTimelineView::_gui_input);
}
