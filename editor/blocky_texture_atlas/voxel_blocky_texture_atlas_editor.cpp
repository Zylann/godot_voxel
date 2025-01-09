#include "voxel_blocky_texture_atlas_editor.h"
#include "../../constants/voxel_string_names.h"
#include "../../meshers/blocky/blocky_connected_textures.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/confirmation_dialog.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/h_split_container.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/input_event_mouse_button.h"
#include "../../util/godot/classes/input_event_mouse_motion.h"
#include "../../util/godot/classes/item_list.h"
#include "../../util/godot/classes/line_edit.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/classes/texture_rect.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/classes/v_separator.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/mouse_button.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/inspector/inspector.h"
#include "../../util/godot/pan_zoom_container.h"
#include "../../util/math/vector2i.h"

namespace zylann::voxel {

// TODO Figure out a generic approach to blob tilesets.
// Instead of expecting our own 12x4 layout (as seen from OptiFine CTM), allow the user to multi-select tiles, create a
// Blob9 tile, then allow to define the layout by drawing a pattern of connections over the existing layout, so we can
// then recognize which tiles are available and validate it?
// This sounds feasible but it's a bunch of work, for now we'll support only the fixed layout.

namespace {
enum ContextMenuID {
	MENU_CREATE_SINGLE_TILE,
	MENU_CREATE_GRID_OF_TILES,
	MENU_CREATE_BLOB9_TILE,
	MENU_CREATE_RANDOM_TILE,
	MENU_CREATE_EXTENDED_TILE,
	MENU_REMOVE_TILE,
	MENU_RENAME_TILE
};
}

VoxelBlockyTextureAtlasEditor::VoxelBlockyTextureAtlasEditor() {
	const float editor_scale = EDSCALE;

	HSplitContainer *split_container = memnew(HSplitContainer);
	split_container->set_split_offset(editor_scale * 200.f);
	split_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	const VoxelStringNames &sn = VoxelStringNames::get_singleton();

	{
		VBoxContainer *vb = memnew(VBoxContainer);
		_tile_list = memnew(ItemList);
		_tile_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		_tile_list->connect(
				sn.item_selected, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_tile_list_item_selected)
		);
		_tile_list->connect(
				sn.item_clicked, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_tile_list_item_clicked)
		);
		vb->add_child(_tile_list);
		split_container->add_child(vb);
	}
	{
		HSplitContainer *split_container2 = memnew(HSplitContainer);

		{
			VBoxContainer *mid_container = memnew(VBoxContainer);
			// So the parent split container will resize this control upon resizing, instead of the one on the right
			mid_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

			{
				_toolbar_container = memnew(HBoxContainer);

				Ref<ButtonGroup> mode_button_group;
				mode_button_group.instantiate();

				{
					Button *button = memnew(Button);
					button->set_toggle_mode(true);
					button->set_tooltip_text("Select Existing Tiles");
					button->set_button_group(mode_button_group);
					button->set_theme_type_variation(sn.FlatButton);
					button->set_meta(sn.mode, MODE_SELECT);
					_toolbar_container->add_child(button);
					_mode_buttons[MODE_SELECT] = button;
				}
				{
					Button *button = memnew(Button);
					button->set_toggle_mode(true);
					button->set_tooltip_text("Create New Tiles");
					button->set_button_group(mode_button_group);
					button->set_theme_type_variation(sn.FlatButton);
					button->set_meta(sn.mode, MODE_CREATE);
					_toolbar_container->add_child(button);
					_mode_buttons[MODE_CREATE] = button;
				}

				_mode_buttons[_mode]->set_pressed(true);

				mode_button_group->connect(
						sn.pressed, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_mode_button_group_pressed)
				);

				_toolbar_container->add_child(memnew(VSeparator));

				// Might become a mode in the future, to edit connectivity
				_connectivity_button = memnew(Button);
				_connectivity_button->set_toggle_mode(true);
				_connectivity_button->set_tooltip_text("Show Connectivity");
				_connectivity_button->set_theme_type_variation(sn.FlatButton);
				_connectivity_button->connect(
						sn.toggled, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_connectivity_button_toggled)
				);
				_toolbar_container->add_child(_connectivity_button);

				mid_container->add_child(_toolbar_container);
			}
			{
				_pan_zoom_container = memnew(ZN_PanZoomContainer);
				_pan_zoom_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);

				Ref<Image> empty_image = zylann::godot::create_empty_image(16, 16, false, Image::FORMAT_L8);
				empty_image->fill(Color(0.3, 0.3, 0.3));
				_empty_texture = ImageTexture::create_from_image(empty_image);

				_texture_rect = memnew(TextureRect);
				_texture_rect->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
				_texture_rect->connect(
						sn.draw, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_texture_rect_draw)
				);
				_texture_rect->connect(
						sn.gui_input, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_texture_rect_gui_input)
				);
				// _texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP);
				_pan_zoom_container->add_child(_texture_rect);

				mid_container->add_child(_pan_zoom_container);
			}

			split_container2->add_child(mid_container);
		}
		{
			_inspector = memnew(ZN_Inspector);
			_inspector->set_custom_minimum_size(Vector2(100, 0));
			_inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			split_container2->add_child(_inspector);
		}

		split_container->add_child(split_container2);
	}

	{
		_select_context_menu = memnew(PopupMenu);
		_select_context_menu->add_item("Rename Tile", MENU_RENAME_TILE);
		_select_context_menu->add_separator();
		_select_context_menu->add_item("Remove Tile", MENU_REMOVE_TILE);
		_select_context_menu->connect(
				sn.id_pressed, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_context_menu_id_pressed)
		);
		add_child(_select_context_menu);
	}
	{
		_create_context_menu = memnew(PopupMenu);
		_create_context_menu->add_item("Create Single Tile", MENU_CREATE_SINGLE_TILE);
		_create_context_menu->add_item("Create Grid of Tiles", MENU_CREATE_GRID_OF_TILES);
		_create_context_menu->add_item("Create Blob9 Tile", MENU_CREATE_BLOB9_TILE);
		_create_context_menu->add_item("Create Random Tile", MENU_CREATE_RANDOM_TILE);
		_create_context_menu->add_item("Create Extended Tile", MENU_CREATE_EXTENDED_TILE);
		_create_context_menu->connect(
				sn.id_pressed, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_context_menu_id_pressed)
		);
		add_child(_create_context_menu);
	}

	add_child(split_container);
}

void VoxelBlockyTextureAtlasEditor::make_read_only() {
	_inspector->hide();
	_toolbar_container->hide();
	_read_only = true;
}

// I'd like rename by clicking on the item like with scene tree nodes, but ItemList doesn't provide that functionality,
// and it's a bit tricky to do. Not impossible, but for now let's use a safe approach
void VoxelBlockyTextureAtlasEditor::open_rename_dialog() {
	ZN_ASSERT_RETURN(_atlas.is_valid());

	const int tile_id = get_selected_tile_id();
	ZN_ASSERT_RETURN(tile_id >= 0);

	if (_rename_popup == nullptr) {
		_rename_popup = memnew(ConfirmationDialog);
		_rename_popup->connect(
				VoxelStringNames::get_singleton().confirmed,
				callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_rename_popup_confirmed)
		);

		_rename_line_edit = memnew(LineEdit);
		_rename_popup->add_child(_rename_line_edit);

		add_child(_rename_popup);
	}

	const String old_name = _atlas->get_tile_name(tile_id);

	_rename_line_edit->set_text(old_name);
	_rename_line_edit->select_all();

	_rename_popup->set_title(String("Rename tile \"{0}\"").format(varray(old_name)));
	_rename_popup->popup_centered();

	_rename_line_edit->grab_focus();
}

void VoxelBlockyTextureAtlasEditor::set_atlas(Ref<VoxelBlockyTextureAtlas> atlas) {
	if (atlas == _atlas) {
		return;
	}

	const VoxelStringNames &sn = VoxelStringNames::get_singleton();

	if (_atlas.is_valid()) {
		_atlas->disconnect(sn.changed, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_atlas_changed));
	}

	_atlas = atlas;

	if (_atlas.is_valid()) {
		_atlas->connect(sn.changed, callable_mp(this, &VoxelBlockyTextureAtlasEditor::on_atlas_changed));
	}

	on_atlas_changed();
	set_selected_tile_id(-1, true);
	set_tile_selection_rect(Rect2i());
}

int VoxelBlockyTextureAtlasEditor::get_selected_tile_id() const {
	const PackedInt32Array selection = _tile_list->get_selected_items();
	if (selection.size() == 0) {
		return -1;
	}
	const int i = selection[0];
	const int tile_id = _tile_list->get_item_metadata(i);
	return tile_id;
}

static void draw_grid(
		CanvasItem &ci,
		const Vector2 origin,
		const Vector2i grid_size,
		const Vector2 spacing,
		const Color color
) {
	for (int y = 0; y < grid_size.y + 1; ++y) {
		ci.draw_line(
				origin + Vector2(0, y * spacing.y), //
				origin + Vector2(grid_size.x * spacing.x, y * spacing.y),
				color
		);
	}
	for (int x = 0; x < grid_size.x + 1; ++x) {
		ci.draw_line(
				origin + Vector2(x * spacing.x, 0), //
				origin + Vector2(x * spacing.x, grid_size.y * spacing.y),
				color
		);
	}
}

void VoxelBlockyTextureAtlasEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			const VoxelStringNames &sn = VoxelStringNames::get_singleton();
			{
				Ref<Texture2D> icon = get_theme_icon(sn.ToolSelect, sn.EditorIcons);
				_mode_buttons[MODE_SELECT]->set_button_icon(icon);
			}
			{
				Ref<Texture2D> icon = get_theme_icon(sn.RegionEdit, sn.EditorIcons);
				_mode_buttons[MODE_CREATE]->set_button_icon(icon);
			}
			{
				Ref<Texture2D> icon = get_theme_icon(sn.TerrainMatchCornersAndSides, sn.EditorIcons);
				_connectivity_button->set_button_icon(icon);
			}
		} break;

		default:
			break;
	}
}

static void draw_blob9_connection_mask(
		CanvasItem &ci,
		const Vector2 origin,
		const Vector2 ts,
		const uint16_t mask,
		const Color color0,
		const Color color1
) {
	const Vector2 tstv = ts / 3.0;
	unsigned int i = 0;
	for (unsigned int sy = 0; sy < 3; ++sy) {
		for (unsigned int sx = 0; sx < 3; ++sx) {
			const Vector2 pos = Vector2(sx, sy) * tstv;
			const Rect2 rect(origin + pos, tstv);
			if (sx == 1 && sy == 1) {
				continue;
			}
			const Color color = (mask & (1 << i)) != 0 ? color1 : color0;
			ci.draw_rect(rect, color, true);
			i += 1;
		}
	}
	if (mask == 0) {
		ci.draw_rect(Rect2(origin + tstv, tstv), color1, true);
	}
}

void VoxelBlockyTextureAtlasEditor::on_texture_rect_draw() {
	if (!_atlas.is_valid()) {
		return;
	}
	CanvasItem &ci = *_texture_rect;

	const Vector2i ts = math::max(_atlas->get_default_tile_resolution(), Vector2i(1, 1));
	const Vector2i res = _atlas->get_resolution();
	const Vector2i num_tiles = res / ts;

	const Color grid_color(0, 0, 0, 0.15);
	const Color group_color(1, 1, 1, 0.25);
	const Color hover_color(0.5, 0.5, 1.0, 0.5);
	const Color selection_color(0.5, 0.5, 1.0, 0.5);
	const Color connection_mask_0_color(0.0, 0.0, 0.0, 0.2);
	const Color connection_mask_1_color(1.0, 1.0, 1.0, 0.2);

	draw_grid(ci, Vector2(), num_tiles, Vector2(ts), grid_color);

	const Span<const VoxelBlockyTextureAtlas::Tile> tiles = _atlas->get_tiles();

	if (_connectivity_button->is_pressed()) {
		for (const VoxelBlockyTextureAtlas::Tile &tile : tiles) {
			if (tile.is_tombstone()) {
				continue;
			}
			if (tile.type == VoxelBlockyTextureAtlas::TILE_TYPE_BLOB9) {
				const Vector2i layout_origin(tile.position_x, tile.position_y);

				// This assumes the default layout!

				for (unsigned int gy = 0; gy < tile.group_size_y; ++gy) {
					for (unsigned int gx = 0; gx < tile.group_size_x; ++gx) {
						const uint8_t case_index = gx + gy * blocky::BLOB9_DEFAULT_LAYOUT_SIZE_X;
						if (case_index >= blocky::BLOB9_TILE_COUNT) {
							// The last tile is not used
							continue;
						}
						const unsigned int mask = blocky::get_connection_mask_from_case_index(case_index);
						const Vector2i pos = layout_origin + Vector2i(gx, gy) * ts;
						draw_blob9_connection_mask(
								ci, Vector2(pos), Vector2(ts), mask, connection_mask_0_color, connection_mask_1_color
						);
					}
				}
			}
		}
	}

	for (const VoxelBlockyTextureAtlas::Tile &tile : tiles) {
		if (tile.is_tombstone()) {
			continue;
		}
		ci.draw_rect(
				Rect2(Vector2(tile.position_x, tile.position_y),
					  Vector2(tile.group_size_x, tile.group_size_y) * Vector2(ts)),
				group_color,
				false
		);
	}

	switch (_mode) {
		case MODE_SELECT: {
			const int tile_id = get_selected_tile_id();
			if (tile_id != -1) {
				const VoxelBlockyTextureAtlas::Tile &tile = _atlas->get_tile(tile_id);
				ci.draw_rect(
						Rect2(Vector2(tile.position_x, tile.position_y),
							  Vector2(tile.group_size_x, tile.group_size_y) * Vector2(ts)),
						selection_color,
						false,
						2.f
				);
			}

			if (_hovered_tile_id != -1) {
				const VoxelBlockyTextureAtlas::Tile &tile = _atlas->get_tile(_hovered_tile_id);
				ci.draw_rect(
						Rect2(Vector2(tile.position_x, tile.position_y),
							  Vector2(tile.group_size_x, tile.group_size_y) * Vector2(ts)),
						hover_color,
						false
				);
			}
		} break;

		case MODE_CREATE: {
			if (Rect2i(Vector2i(), num_tiles).has_point(_hovered_tile_position)) {
				ci.draw_rect(Rect2(Vector2(_hovered_tile_position * ts), Vector2(ts)), hover_color, false);
			}

			if (_tile_selection_rect != Rect2i()) {
				ci.draw_rect(
						Rect2(_tile_selection_rect.position * ts, _tile_selection_rect.size * ts),
						selection_color,
						false,
						2.f
				);
			}
		} break;
	}
}

void VoxelBlockyTextureAtlasEditor::on_atlas_changed() {
	// TODO Not sure we should handle null atlas here
	if (_atlas.is_null()) {
		update_texture_rect();
		_inspector->clear();
		update_tile_list();
		return;
	}

	switch (_atlas->get_last_change()) {
		case VoxelBlockyTextureAtlas::CHANGE_TEXTURE: {
			const Vector2i size(_atlas->get_resolution());
			_pan_zoom_container->set_content_rect(Rect2(Vector2(), Vector2(size)));

			update_texture_rect();
		} break;

		case VoxelBlockyTextureAtlas::CHANGE_TILE_ADDED:
		case VoxelBlockyTextureAtlas::CHANGE_TILE_REMOVED:
			update_tile_list();
			_texture_rect->queue_redraw();
			break;

		case VoxelBlockyTextureAtlas::CHANGE_TILE_MODIFIED:
			_texture_rect->queue_redraw();
			break;

		default:
			ZN_PRINT_ERROR("Unhandled change");
			break;
	}
}

inline bool is_tile_rect_valid(const Rect2i rect) {
	return rect != Rect2i() && rect.position.x >= 0 && rect.position.y >= 0 && rect.size.x >= 1 && rect.size.y >= 1;
}

void VoxelBlockyTextureAtlasEditor::on_context_menu_id_pressed(int id) {
	ERR_FAIL_COND(_atlas.is_null());
	const Vector2i ts = _atlas->get_default_tile_resolution();

	switch (id) {
		case MENU_CREATE_SINGLE_TILE: {
			const Rect2i rect = _tile_selection_rect;
			ZN_ASSERT_RETURN(is_tile_rect_valid(rect));

			VoxelBlockyTextureAtlas::Tile tile;
			tile.type = blocky::TILE_SINGLE;
			tile.position_x = rect.position.x * ts.x;
			tile.position_y = rect.position.y * ts.y;
			tile.group_size_x = 1;
			tile.group_size_y = 1;

			// TODO UndoRedo
			_atlas->add_tile(tile);
			on_atlas_changed();
		} break;

		case MENU_CREATE_GRID_OF_TILES: {
			ZN_ASSERT_RETURN(is_tile_rect_valid(_tile_selection_rect));

			Vector2i tpos;
			const Vector2i origin = _tile_selection_rect.position * ts;
			for (tpos.y = 0; tpos.y < _tile_selection_rect.size.y; ++tpos.y) {
				for (tpos.x = 0; tpos.x < _tile_selection_rect.size.x; ++tpos.x) {
					VoxelBlockyTextureAtlas::Tile tile;
					tile.type = blocky::TILE_SINGLE;
					tile.position_x = origin.x + tpos.x * ts.x;
					tile.position_y = origin.y + tpos.y * ts.y;
					tile.group_size_x = 1;
					tile.group_size_y = 1;

					// TODO UndoRedo
					_atlas->add_tile(tile);
				}
			}

			on_atlas_changed();
		} break;

		case MENU_CREATE_BLOB9_TILE: {
			ZN_ASSERT_RETURN(is_tile_rect_valid(_tile_selection_rect));

			VoxelBlockyTextureAtlas::Tile tile;
			tile.type = blocky::TILE_BLOB9;
			tile.position_x = _tile_selection_rect.position.x * ts.x;
			tile.position_y = _tile_selection_rect.position.y * ts.y;
			tile.group_size_x = blocky::BLOB9_DEFAULT_LAYOUT_SIZE_X;
			tile.group_size_y = blocky::BLOB9_DEFAULT_LAYOUT_SIZE_Y;

			// TODO UndoRedo
			_atlas->add_tile(tile);
			on_atlas_changed();

		} break;

		case MENU_CREATE_RANDOM_TILE: {
			ZN_ASSERT_RETURN(is_tile_rect_valid(_tile_selection_rect));

			VoxelBlockyTextureAtlas::Tile tile;
			tile.type = blocky::TILE_RANDOM;
			tile.position_x = _tile_selection_rect.position.x * ts.x;
			tile.position_y = _tile_selection_rect.position.y * ts.y;
			tile.group_size_x = _tile_selection_rect.size.x;
			tile.group_size_y = _tile_selection_rect.size.y;

			// TODO UndoRedo
			_atlas->add_tile(tile);
			on_atlas_changed();

		} break;

		case MENU_CREATE_EXTENDED_TILE: {
			ZN_ASSERT_RETURN(is_tile_rect_valid(_tile_selection_rect));

			VoxelBlockyTextureAtlas::Tile tile;
			tile.type = blocky::TILE_EXTENDED;
			tile.position_x = _tile_selection_rect.position.x * ts.x;
			tile.position_y = _tile_selection_rect.position.y * ts.y;
			tile.group_size_x = _tile_selection_rect.size.x;
			tile.group_size_y = _tile_selection_rect.size.y;

			// TODO UndoRedo
			_atlas->add_tile(tile);
			on_atlas_changed();

		} break;

		case MENU_RENAME_TILE:
			open_rename_dialog();
			break;

		case MENU_REMOVE_TILE: {
			const int tile_id = get_selected_tile_id();
			ZN_ASSERT_RETURN(tile_id >= 0);

			// TODO UndoRedo
			_atlas->remove_tile(tile_id);
			on_atlas_changed();
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled menu item");
			break;
	}
}

void VoxelBlockyTextureAtlasEditor::on_tile_list_item_selected(int item_index) {
	ERR_FAIL_COND(_atlas.is_null());
	const int tile_id = _tile_list->get_item_metadata(item_index);
	set_selected_tile_id(tile_id, false);
}

static void open_popup_at_mouse(PopupMenu *popup, Control *from_control) {
	// Not sure if it has to be that complicated to open a context menu where the mouse is?
	const Vector2 mouse_pos_in_editor =
			from_control->get_screen_position() + from_control->get_local_mouse_position() * from_control->get_scale();
	popup->set_position(mouse_pos_in_editor);
	popup->popup();
}

void VoxelBlockyTextureAtlasEditor::on_tile_list_item_clicked(
		int item_index,
		Vector2 at_position,
		int mouse_button_index
) {
	if (_read_only) {
		return;
	}
	if (static_cast<MouseButton>(mouse_button_index) == ZN_GODOT_MouseButton_RIGHT) {
		ERR_FAIL_COND(_atlas.is_null());
		const int tile_id = _tile_list->get_item_metadata(item_index);
		set_selected_tile_id(tile_id, false);
		open_popup_at_mouse(_select_context_menu, _tile_list);
	}
}

int VoxelBlockyTextureAtlasEditor::get_tile_list_index_from_tile_id(const int tile_id_to_find) const {
	const int item_count = _tile_list->get_item_count();
	for (int i = 0; i < item_count; ++i) {
		const int id = _tile_list->get_item_metadata(i);
		if (id == tile_id_to_find) {
			return i;
		}
	}
	return -1;
}

void VoxelBlockyTextureAtlasEditor::set_selected_tile_id(const int tile_id_to_select, const bool update_list) {
	// Don't early-return if the selection comes from the list because that's where we take the selected tile from... a
	// bit ugly
	if (update_list && tile_id_to_select == get_selected_tile_id()) {
		return;
	}

	if (tile_id_to_select == -1) {
		_tile_list->deselect_all();
		set_tile_selection_rect(Rect2i());
		update_inspector(-1);

	} else {
		ERR_FAIL_COND(_atlas.is_null());

		if (update_list) {
			const int i = get_tile_list_index_from_tile_id(tile_id_to_select);
			if (i >= 0) {
				_tile_list->select(i);
				_tile_list->ensure_current_is_visible();
			} else {
				ZN_PRINT_ERROR("Tile not found in list. Bug?");
			}
		}

		const VoxelBlockyTextureAtlas::Tile &tile = _atlas->get_tile(tile_id_to_select);
		const Vector2i ts = _atlas->get_default_tile_resolution();
		set_tile_selection_rect(
				Rect2i(Vector2i(tile.position_x, tile.position_y) / ts, Vector2i(tile.group_size_x, tile.group_size_y))
		);

		update_inspector(tile_id_to_select);
	}
}

void VoxelBlockyTextureAtlasEditor::update_inspector(const int tile_index) {
	_inspector->clear();

	if (tile_index == -1) {
		return;
	}

	ZN_ASSERT_RETURN(tile_index >= 0);
	ZN_ASSERT_RETURN(_atlas.is_valid());

	_inspector->set_target_object(_atlas.ptr());
	_inspector->set_target_index(tile_index);

	// _inspector->add_indexed_property("Name", "set_tile_name", "get_tile_name");
	_inspector->add_indexed_property(
			"Type", "set_tile_type", "get_tile_type", String(VoxelBlockyTextureAtlas::TILE_TYPE_HINT_STRING), false
	);

	_inspector->add_indexed_property(
			"Position",
			"set_tile_position",
			"get_tile_position",
			VoxelBlockyTextureAtlas::get_supported_tile_position_range()
	);

	const VoxelBlockyTextureAtlas::TileType type = _atlas->get_tile_type(tile_index);
	_inspector->add_indexed_property(
			"Group Size",
			"set_tile_group_size",
			"get_tile_group_size",
			VoxelBlockyTextureAtlas::get_supported_tile_group_size_range(),
			type == VoxelBlockyTextureAtlas::TILE_TYPE_EXTENDED || type == VoxelBlockyTextureAtlas::TILE_TYPE_RANDOM
	);

	if (type == VoxelBlockyTextureAtlas::TILE_TYPE_RANDOM) {
		_inspector->add_indexed_property("Random Rotation", "set_tile_random_rotation", "get_tile_random_rotation");
	}
}

void VoxelBlockyTextureAtlasEditor::update_texture_rect() {
	if (_atlas.is_valid()) {
		Ref<Texture2D> texture = _atlas->get_texture();
		if (texture.is_null()) {
			texture = _empty_texture;
		}
		_texture_rect->set_texture(texture);
		_texture_rect->set_size(Vector2(_atlas->get_resolution()));
	} else {
		_texture_rect->set_texture(Ref<Texture>());
	}
	_texture_rect->queue_redraw();
}

void VoxelBlockyTextureAtlasEditor::update_tile_list() {
	if (_atlas.is_null()) {
		_tile_list->clear();
		return;
	}

	Span<const VoxelBlockyTextureAtlas::Tile> tiles = _atlas->get_tiles();

	// Save selection
	StdVector<int> selected_tile_indices;
	{
		const PackedInt32Array selected_items = _tile_list->get_selected_items();
		const Span<const int32_t> selected_items_s = to_span(selected_items);
		for (const int32_t item_index : selected_items_s) {
			const int tile_id = _tile_list->get_item_metadata(item_index);
			selected_tile_indices.push_back(tile_id);
		}
	}

	// Repopulate list
	_tile_list->clear();
	for (unsigned int tile_index = 0; tile_index < tiles.size(); ++tile_index) {
		const VoxelBlockyTextureAtlas::Tile &tile = tiles[tile_index];
		if (tile.group_size_x == 0) {
			// Tombstone
			continue;
		}

		String item_title;
		if (tile.name.size() == 0) {
			item_title = String("<unnamed{0}>").format(varray(tile_index));
		} else {
			item_title = tile.name.c_str();
		}

		const int item_index = _tile_list->add_item(item_title);
		_tile_list->set_item_metadata(item_index, tile_index);
	}

	// Restore selection
	if (selected_tile_indices.size() > 0) {
		const int item_count = _tile_list->get_item_count();
		for (int item_index = 0; item_index < item_count; ++item_index) {
			const int tile_index = _tile_list->get_item_metadata(item_index);
			if (contains(selected_tile_indices, tile_index)) {
				// Note: `select` does not trigger item selection signals.
				if (selected_tile_indices.size() == 1) {
					_tile_list->select(item_index, true);
					break;
				} else {
					_tile_list->select(item_index, false);
				}
			}
		}
	}
}

void VoxelBlockyTextureAtlasEditor::set_tile_selection_rect_from_pixel_positions(const Vector2 p0, const Vector2 p1) {
	ERR_FAIL_COND(_atlas.is_null());
	const Vector2i ts = _atlas->get_default_tile_resolution();
	ERR_FAIL_COND(ts.x <= 0 || ts.y <= 0);

	const Vector2i pi0(p0);
	const Vector2i pi1(p1);

	const Vector2i minp = math::clamp(math::min(pi0, pi1), Vector2i(), _atlas->get_resolution());
	const Vector2i maxp = math::clamp(math::max(pi0, pi1), Vector2i(), _atlas->get_resolution());

	const Vector2i tminp = minp / ts;
	const Vector2i tmaxp = math::ceildiv(maxp, ts);

	const Vector2i tsize = math::max(tmaxp - tminp, Vector2i(1, 1));

	const Rect2i tile_rect(tminp, tsize);

	set_tile_selection_rect(tile_rect);
}

void VoxelBlockyTextureAtlasEditor::set_tile_selection_rect_from_pixel_position(const Vector2 p0) {
	ERR_FAIL_COND(_atlas.is_null());
	const Vector2i ts = _atlas->get_default_tile_resolution();
	ERR_FAIL_COND(ts.x <= 0 || ts.y <= 0);

	const Vector2i pi0(p0);

	const Vector2i minp = math::clamp(pi0, Vector2i(), _atlas->get_resolution());

	const Vector2i tpos0 = minp / ts;

	const Rect2i tile_rect(tpos0, Vector2i(1, 1));

	set_tile_selection_rect(tile_rect);
}

void VoxelBlockyTextureAtlasEditor::set_tile_selection_rect(const Rect2i rect) {
	if (rect != _tile_selection_rect) {
		_tile_selection_rect = rect;
		_texture_rect->queue_redraw();
	}
}

void VoxelBlockyTextureAtlasEditor::set_hovered_tile_id(const int id) {
	if (id != _hovered_tile_id) {
		_hovered_tile_id = id;
		_texture_rect->queue_redraw();
	}
}

void VoxelBlockyTextureAtlasEditor::set_hovered_tile_position(const Vector2i pos) {
	if (pos != _hovered_tile_position) {
		_hovered_tile_position = pos;
		_texture_rect->queue_redraw();
	}
}

void VoxelBlockyTextureAtlasEditor::on_texture_rect_gui_input(Ref<InputEvent> event) {
	if (_atlas.is_null()) {
		return;
	}

	switch (_mode) {
		case MODE_SELECT: {
			Ref<InputEventMouseMotion> mouse_motion_event = event;
			if (mouse_motion_event.is_valid()) {
				const Vector2 pos = mouse_motion_event->get_position();
				const Vector2i posi = Vector2i(pos);
				const Vector2i ts = _atlas->get_default_tile_resolution();
				set_hovered_tile_id(_atlas->get_tile_id_at_pixel_position(posi));
				return;
			}

			Ref<InputEventMouseButton> mouse_button_event = event;
			if (mouse_button_event.is_valid()) {
				if (mouse_button_event->is_pressed()) {
					switch (mouse_button_event->get_button_index()) {
						case ZN_GODOT_MouseButton_LEFT: {
							if (_hovered_tile_id != -1) {
								set_selected_tile_id(_hovered_tile_id, true);
							}
						} break;

						case ZN_GODOT_MouseButton_RIGHT: {
							if (_hovered_tile_id != -1) {
								set_selected_tile_id(_hovered_tile_id, true);
							}
							if (!_read_only) {
								open_popup_at_mouse(_select_context_menu, _texture_rect);
							}
						} break;

						default:
							break;
					}
				}
				return;
			}
		} break;

		case MODE_CREATE: {
			Ref<InputEventMouseMotion> mouse_motion_event = event;
			if (mouse_motion_event.is_valid()) {
				const Vector2 pos = mouse_motion_event->get_position();
				const Vector2i posi = Vector2i(pos);
				const Vector2i ts = _atlas->get_default_tile_resolution();
				set_hovered_tile_position(posi / ts);

				const Vector2 mouse_pos = mouse_motion_event->get_position();

				if (_pressed) {
					set_tile_selection_rect_from_pixel_positions(_mouse_press_pos, mouse_pos);
				}

				return;
			}

			Ref<InputEventMouseButton> mouse_button_event = event;
			if (mouse_button_event.is_valid()) {
				if (mouse_button_event->is_pressed()) {
					switch (mouse_button_event->get_button_index()) {
						case ZN_GODOT_MouseButton_LEFT: {
							_mouse_press_pos = mouse_button_event->get_position();
							_pressed = true;
							set_tile_selection_rect_from_pixel_position(_mouse_press_pos);
						} break;

						case ZN_GODOT_MouseButton_RIGHT: {
							const Vector2i mouse_pos_px(mouse_button_event->get_position());
							const int tile_id = _atlas->get_tile_id_at_pixel_position(mouse_pos_px);

							if (tile_id == -1) {
								if (Rect2i(Vector2i(), _atlas->get_resolution()).has_point(mouse_pos_px)) {
									if (_tile_selection_rect == Rect2i()) {
										set_tile_selection_rect(Rect2i(_hovered_tile_position, Vector2i(1, 1)));
									}
									if (!_read_only) {
										open_popup_at_mouse(_create_context_menu, _texture_rect);
									}
								}
							} else {
								_tile_selection_rect = Rect2i();
								// TODO Menu to delete hovered tile?
							}
						} break;

						default:
							break;
					}
				} else {
					_pressed = false;
				}
				return;
			}
		} break;
	}
}

void VoxelBlockyTextureAtlasEditor::on_mode_button_group_pressed(BaseButton *pressed_button) {
	ERR_FAIL_COND(pressed_button == nullptr);
	const int mode = pressed_button->get_meta(VoxelStringNames::get_singleton().mode);
	ERR_FAIL_COND(mode < 0 || mode >= MODE_COUNT);
	_mode = static_cast<Mode>(mode);
	set_tile_selection_rect(Rect2i());
}

void VoxelBlockyTextureAtlasEditor::on_rename_popup_confirmed() {
	const String new_name = _rename_line_edit->get_text().strip_edges();

	if (new_name.is_empty()) {
		ZN_PRINT_ERROR("Name cannot be empty");
		return;
	}

	ZN_ASSERT_RETURN(_atlas.is_valid());

	const int tile_id = get_selected_tile_id();
	ZN_ASSERT_RETURN(tile_id >= 0);

	// TODO UndoRedo
	_atlas->set_tile_name(tile_id, new_name);

	const int item_index = get_tile_list_index_from_tile_id(tile_id);
	_tile_list->set_item_text(item_index, new_name);
}

void VoxelBlockyTextureAtlasEditor::on_connectivity_button_toggled(bool pressed) {
	_texture_rect->queue_redraw();
}

void VoxelBlockyTextureAtlasEditor::_bind_methods() {
	//
}

} // namespace zylann::voxel
