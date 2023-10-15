#include "voxel_generator_multipass_cache_viewer.h"
#include "../../constants/voxel_string_names.h"
#include "../../engine/voxel_engine.h"
#include "../../util/godot/classes/font.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

namespace {
const uint64_t UPDATE_INTERVAL_MS = 200;
const int TILE_SIZE = 3;

const Color g_empty_color(0.1, 0.1, 0.1);

// static_assert(VoxelGeneratorMultipassCB::MAX_SUBPASSES == 7);
static const std::array<Color, VoxelGeneratorMultipassCB::MAX_SUBPASSES> g_subpass_colors = {
	Color(0.7, 0.7, 0.7), // 0
	Color(0.5, 0.1, 0.1), // 1
	Color(0.8, 0.2, 0.2), // 2
	Color(0.1, 0.5, 0.1), // 3
	Color(0.2, 0.8, 0.2), // 4
	Color(0.2, 0.2, 0.5), // 5
	Color(0.4, 0.4, 0.9), // 6
};

} // namespace

VoxelGeneratorMultipassCacheViewer::VoxelGeneratorMultipassCacheViewer() {
	set_custom_minimum_size(Vector2(0, EDSCALE * 128.0));
	set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
}

void VoxelGeneratorMultipassCacheViewer::set_generator(Ref<VoxelGeneratorMultipassCB> generator) {
	_generator = generator;
}

void VoxelGeneratorMultipassCacheViewer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED:
			set_process(is_visible());
			break;

		case NOTIFICATION_PROCESS:
			process();
			break;

		case NOTIFICATION_DRAW:
			draw();
			break;

		default:
			break;
	}
}

void VoxelGeneratorMultipassCacheViewer::process() {
	if (is_visible_in_tree()) {
		const uint64_t now = Time::get_singleton()->get_ticks_msec();
		if (now >= _next_update_time) {
			update_image();
			queue_redraw();
			_next_update_time = now + UPDATE_INTERVAL_MS;
		}
	}
}

void VoxelGeneratorMultipassCacheViewer::update_image() {
	if (_generator.is_null()) {
		return;
	}

	Rect2i view_rect_tiles(Vector2i(), get_size() / TILE_SIZE);
	// TODO Usability: maybe have a way to control where we look at
	view_rect_tiles.position -= view_rect_tiles.size / 2;

	const Color bg_color(0.3, 0.3, 0.3);

	if (_image.is_null() || //
			_image->get_width() != view_rect_tiles.size.x || //
			_image->get_height() != view_rect_tiles.size.y) {
		_image = create_empty_image(view_rect_tiles.size.x, view_rect_tiles.size.y, false, Image::FORMAT_RGB8);
		_image->fill(bg_color);
	}

	const Color err_color(1.0, 0.0, 1.0);

	if (_generator->debug_try_get_column_states(_debug_column_states)) {
		_image->fill(bg_color);

		for (const VoxelGeneratorMultipassCB::DebugColumnState &column : _debug_column_states) {
			if (!view_rect_tiles.has_point(column.position)) {
				continue;
			}

			Color color;
			if (column.subpass_index == -1) {
				color = g_empty_color;
			} else if (column.subpass_index >= 0 && column.subpass_index < int(g_subpass_colors.size())) {
				color = g_subpass_colors[column.subpass_index];
			} else {
				color = err_color;
			}

			// Debug refcounts
			// if (column.viewer_count == 0) {
			// 	color = Color(0, 0, 0);
			// } else if (column.viewer_count == 1) {
			// 	color = Color(0.5, 0, 0);
			// } else if (column.viewer_count == 2) {
			// 	color = Color(1.0, 0, 0);
			// } else if (column.viewer_count == 3) {
			// 	color = Color(1.0, 0.5, 0);
			// } else if (column.viewer_count == 4) {
			// 	color = Color(1.0, 1.0, 0);
			// } else {
			// 	color = Color(1.0, 1.0, 1.0);
			// }

			const Vector2i pixel_pos = column.position - view_rect_tiles.position;
			// TODO Optimize: access pixels directly so we can use exact 8-bit color components.
			// `set_pixel` has unnecessary overhead. On top of that, it would more conveniently allow us to diff the
			// previous image, therefore knowing if anything changed, and then preventing from redrawing the control,
			// which in turns prevents redrawing the entire Godot Editor
			_image->set_pixelv(pixel_pos, color);
		}
	}

	ZN_ASSERT_RETURN(_image.is_valid());
	if (_texture.is_null() || Vector2i(_texture->get_size()) != _image->get_size()) {
		_texture = ImageTexture::create_from_image(_image);
	} else {
		_texture->update(_image);
	}
}

void VoxelGeneratorMultipassCacheViewer::draw() {
	ZN_PROFILE_SCOPE();

	if (_texture.is_valid()) {
		draw_texture_rect(_texture, Rect2(Vector2(), _texture->get_size() * TILE_SIZE), false);
	}

	if (_generator.is_valid()) {
		const VoxelStringNames &sn = VoxelStringNames::get_singleton();

		Ref<Font> font = get_theme_font(sn.font, sn.Label);
		const int font_size = get_theme_font_size(sn.font_size, sn.Label);
		const Color text_color = get_theme_color(sn.font_color, sn.Editor) * Color(1, 1, 1, 0.7);

		const float font_ascent = font->get_ascent(font_size);
		const int line_height = font->get_height(font_size);

		const int color_rect_width = (line_height * 4) / 5;
		const float color_rect_pad = EDSCALE;

		const int pass_count = _generator->get_pass_count();

		Vector2 anchor_pos(EDSCALE, EDSCALE);
		{
			Vector2 pos = anchor_pos;

			const Rect2 color_rect = Rect2(pos, Vector2(color_rect_width, line_height)).grow(-color_rect_pad);
			draw_rect(color_rect, g_empty_color);

			pos.x += color_rect.size.x * 1.2f;
			pos.y += font_ascent;

			draw_string(font, pos, "Empty", HORIZONTAL_ALIGNMENT_LEFT, -1.f, font_size, text_color);

			anchor_pos.y += line_height;
		}
		for (int pass_index = 0; pass_index < pass_count; ++pass_index) {
			Vector2 pos = anchor_pos;

			const Rect2 color_rect = Rect2(pos, Vector2(color_rect_width, line_height)).grow(-color_rect_pad);

			if (pass_index == 0) {
				draw_rect(color_rect, g_subpass_colors[0]);

			} else {
				const int subpass_index0 = pass_index * 2 - 1;
				const int subpass_index1 = subpass_index0 + 1;

				const Color color0 = g_subpass_colors[subpass_index0];
				const Color color1 = g_subpass_colors[subpass_index1];

				const Rect2 rect0(color_rect.position, Vector2(color_rect.size.x, color_rect.size.y / 2.0));
				const Rect2 rect1(Vector2(rect0.position.x, rect0.position.y + rect0.size.y), rect0.size);

				draw_rect(rect0, color0);
				draw_rect(rect1, color1);
			}

			pos.x += color_rect.size.x * 1.2f;
			pos.y += font_ascent;

			draw_string(font, pos, String("Pass {0}").format(varray(pass_index)), HORIZONTAL_ALIGNMENT_LEFT, -1.f,
					font_size, text_color);

			anchor_pos.y += line_height;
		}
	}
}

} // namespace zylann::voxel
