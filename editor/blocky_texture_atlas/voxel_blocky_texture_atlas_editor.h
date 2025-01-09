#ifndef VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_H
#define VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_H

#include "../../meshers/blocky/voxel_blocky_texture_atlas.h"
#include "../../util/godot/classes/image_texture.h"
#include "../../util/godot/classes/margin_container.h"
#include "../../util/godot/macros.h"
#include <array>

ZN_GODOT_FORWARD_DECLARE(class TextureRect);
ZN_GODOT_FORWARD_DECLARE(class PopupMenu);
ZN_GODOT_FORWARD_DECLARE(class ItemList);
ZN_GODOT_FORWARD_DECLARE(class ConfirmationDialog);
ZN_GODOT_FORWARD_DECLARE(class Button);
ZN_GODOT_FORWARD_DECLARE(class BaseButton);
ZN_GODOT_FORWARD_DECLARE(class LineEdit);
ZN_GODOT_FORWARD_DECLARE(class HBoxContainer);

namespace zylann {

class ZN_PanZoomContainer;
class ZN_Inspector;

namespace voxel {

class VoxelBlockyTextureAtlasEditor : public MarginContainer {
	GDCLASS(VoxelBlockyTextureAtlasEditor, MarginContainer)
public:
	VoxelBlockyTextureAtlasEditor();

	void set_atlas(Ref<VoxelBlockyTextureAtlas> atlas);
	int get_selected_tile_id() const;
	void make_read_only();

private:
	static void _bind_methods();

	void _notification(int p_what);

	void on_texture_rect_draw();
	void on_atlas_changed();
	void on_texture_rect_gui_input(Ref<InputEvent> event);
	void on_context_menu_id_pressed(int id);
	void on_tile_list_item_selected(int item_index);
	void on_tile_list_item_clicked(int item_index, Vector2 at_position, int mouse_button_index);
	void on_mode_button_group_pressed(BaseButton *pressed_button);
	void on_rename_popup_confirmed();
	void on_connectivity_button_toggled(bool pressed);

	void update_texture_rect();
	void update_tile_list();
	void set_tile_selection_rect_from_pixel_positions(const Vector2 p0, const Vector2 p1);
	void set_tile_selection_rect_from_pixel_position(const Vector2 p0);
	void set_tile_selection_rect(const Rect2i rect);
	void set_hovered_tile_id(const int id);
	void set_hovered_tile_position(const Vector2i pos);
	void update_inspector(const int tile_index);
	int get_tile_list_index_from_tile_id(const int tile_id_to_find) const;
	void set_selected_tile_id(const int tile_id_to_select, const bool update_list);
	void open_rename_dialog();

	enum Mode {
		MODE_SELECT,
		MODE_CREATE,
		MODE_COUNT,
	};

	Ref<VoxelBlockyTextureAtlas> _atlas;
	ZN_PanZoomContainer *_pan_zoom_container = nullptr;
	TextureRect *_texture_rect = nullptr;
	Vector2i _hovered_tile_position;
	Rect2i _tile_selection_rect;
	int _hovered_tile_id = -1;
	Ref<ImageTexture> _empty_texture;
	PopupMenu *_select_context_menu = nullptr;
	PopupMenu *_create_context_menu = nullptr;
	ConfirmationDialog *_rename_popup = nullptr;
	LineEdit *_rename_line_edit = nullptr;
	ItemList *_tile_list = nullptr;
	Mode _mode = MODE_SELECT;
	std::array<Button *, MODE_COUNT> _mode_buttons;
	Button *_connectivity_button = nullptr;
	bool _pressed = false;
	Vector2 _mouse_press_pos;
	ZN_Inspector *_inspector = nullptr;
	HBoxContainer *_toolbar_container = nullptr;
	bool _read_only = false;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BLOCKY_TEXTURE_ATLAS_EDITOR_H
