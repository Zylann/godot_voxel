#ifndef VOXEL_ABOUT_WINDOW_H
#define VOXEL_ABOUT_WINDOW_H

#include <scene/gui/dialogs.h>

class TextureRect;

class VoxelAboutWindow : public WindowDialog {
	GDCLASS(VoxelAboutWindow, WindowDialog)
public:
	VoxelAboutWindow();

protected:
	void _notification(int p_what);

private:
	void _on_ok_button_pressed();
	void _on_about_rich_text_label_meta_clicked(Variant meta);

	static void _bind_methods();

	TextureRect *_icon_texture_rect;
};

#endif // VOXEL_ABOUT_WINDOW_H
