#ifndef VOXEL_ABOUT_WINDOW_H
#define VOXEL_ABOUT_WINDOW_H

#include "../util/godot/classes/accept_dialog.h"
#include "../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class TextureRect);
ZN_GODOT_FORWARD_DECLARE(class RichTextLabel);

namespace zylann::voxel {

class VoxelAboutWindow : public AcceptDialog {
	GDCLASS(VoxelAboutWindow, AcceptDialog)
public:
	VoxelAboutWindow();

	// The same window can be shown by more than one plugin, therefore it is created only once internally.
	// It cannot be created in the initialization of the module because the editor isn't available yet.
	static void create_singleton(Node &base_control);
	static void destroy_singleton();
	static void popup_singleton();

protected:
	void _notification(int p_what);

private:
	void _on_about_rich_text_label_meta_clicked(Variant meta);
	void _on_third_party_list_item_selected(int index);

	static void _bind_methods();

	TextureRect *_icon_texture_rect;
	RichTextLabel *_third_party_rich_text_label;
};

} // namespace zylann::voxel

#endif // VOXEL_ABOUT_WINDOW_H
