#include "voxel_generator_multipass_cache_viewer.h"
#include "../../engine/voxel_engine.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

namespace {
const uint64_t UPDATE_INTERVAL_MS = 200;
}

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
			if (is_visible_in_tree()) {
				const uint64_t now = Time::get_singleton()->get_ticks_msec();
				if (now >= _next_update_time) {
					queue_redraw();
					_next_update_time = now + UPDATE_INTERVAL_MS;
				}
			}
			break;

		case NOTIFICATION_DRAW:
			draw();
			break;

		default:
			break;
	}
}

void VoxelGeneratorMultipassCacheViewer::draw() {
	ZN_PROFILE_SCOPE();

	const int tile_size = 3;
	Rect2i view_rect_tiles(Vector2i(), get_size() / tile_size);
	// TODO Usability: maybe have a way to control where we look at
	view_rect_tiles.position -= view_rect_tiles.size / 2;

	const Color bg_color(0.3, 0.3, 0.3);

	if (_image.is_null() || //
			_image->get_width() != view_rect_tiles.size.x || //
			_image->get_height() != view_rect_tiles.size.y) {
		_image = Image::create_empty(view_rect_tiles.size.x, view_rect_tiles.size.y, false, Image::FORMAT_RGB8);
		_image->fill(bg_color);
	}

	FixedArray<Color, VoxelGeneratorMultipassCB::MAX_SUBPASSES> subpass_colors;
	subpass_colors[0] = Color(0.7, 0.7, 0.7);
	subpass_colors[1] = Color(0.5, 0.1, 0.1);
	subpass_colors[2] = Color(0.8, 0.2, 0.2);
	subpass_colors[3] = Color(0.1, 0.5, 0.1);
	subpass_colors[4] = Color(0.2, 0.8, 0.2);
	subpass_colors[5] = Color(0.2, 0.2, 0.5);
	subpass_colors[6] = Color(0.4, 0.4, 0.9);
	static_assert(VoxelGeneratorMultipassCB::MAX_SUBPASSES == 7);
	const Color err_color(1.0, 0.0, 1.0);
	const Color empty_color(0.1, 0.1, 0.1);

	if (_generator->debug_try_get_column_states(_debug_column_states)) {
		_image->fill(bg_color);

		for (const VoxelGeneratorMultipassCB::DebugColumnState &column : _debug_column_states) {
			if (!view_rect_tiles.has_point(column.position)) {
				continue;
			}

			Color color;
			if (column.subpass_index == -1) {
				color = empty_color;
			} else if (column.subpass_index >= 0 && column.subpass_index < int(subpass_colors.size())) {
				color = subpass_colors[column.subpass_index];
			} else {
				color = err_color;
			}

			const Vector2i pixel_pos = column.position - view_rect_tiles.position;
			// TODO Optimize: access pixels directly and use 8-bit color components, `set_pixel` has unnecessary
			// overhead
			_image->set_pixelv(pixel_pos, color);
		}
	}

	ZN_ASSERT_RETURN(_image.is_valid());
	if (_texture.is_null() || Vector2i(_texture->get_size()) != _image->get_size()) {
		_texture = ImageTexture::create_from_image(_image);
	} else {
		_texture->update(_image);
	}

	draw_texture_rect(_texture, Rect2(Vector2(), view_rect_tiles.size * tile_size));
}

} // namespace zylann::voxel
