#include "about_window.h"

#include <core/os/os.h>
#include <editor/editor_scale.h>
#include <scene/gui/rich_text_label.h>
#include <scene/gui/tab_container.h>
#include <scene/gui/texture_rect.h>

VoxelAboutWindow::VoxelAboutWindow() {
	// Generated with the help of https://github.com/Zylann/godot_scene_code_converter

	set_title(TTR("About Voxel Tools"));
	set_resizable(true);
	set_custom_minimum_size(Vector2(600, 300) * EDSCALE);
	set_visible(true);

	VBoxContainer *v_box_container = memnew(VBoxContainer);
	v_box_container->set_anchor(MARGIN_RIGHT, 1);
	v_box_container->set_anchor(MARGIN_BOTTOM, 1);
	v_box_container->set_margin(MARGIN_LEFT, 4 * EDSCALE);
	v_box_container->set_margin(MARGIN_TOP, 4 * EDSCALE);
	v_box_container->set_margin(MARGIN_RIGHT, -4 * EDSCALE);
	v_box_container->set_margin(MARGIN_BOTTOM, -4 * EDSCALE);

	// HB
	HBoxContainer *h_box_container = memnew(HBoxContainer);
	h_box_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	TextureRect *texture_rect = memnew(TextureRect);
	// TODO Can't access ANY icon from here because they all return the default empty icon at this stage...
	//texture_rect->set_texture(get_icon("VoxelTerrainLarge", "EditorIcons"));
	texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	_icon_texture_rect = texture_rect;
	h_box_container->add_child(texture_rect);

	TabContainer *tab_container = memnew(TabContainer);
	tab_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	// About
	String about_text = "[b]Version:[/b] {version}\n"
						"[b]Author:[/b] Marc Gilleron\n"
						"[b]Repository:[/b] [url]https://github.com/Zylann/godot_voxel[/url]\n"
						"[b]Issue tracker:[/b] [url]https://github.com/Zylann/godot_voxel/issues[/url]\n"
						"\n"
						"[b]Donors:[/b]\n"
						"wacyym\n"
						"Sergey Lapin (slapin)\n"
						"Jonas (NoFr1ends)\n"
						"lenis0012\n"
						"Phyronnaz\n"
						"RonanZe\n"
						"furtherorbit\n"
						"jp.owo.Manda (segfault-god)\n"
						"hidemat\n"
						"Jakub Buriánek (Buri)\n"
						"Justin Swanhart (Greenlion)\n"
						"Sebastian Clausen (sclausen)\n";
	{
		Dictionary d;
		// TODO Take version from somewhere unique
		d["version"] = "godot3.3 dev";
		about_text = about_text.format(d);
	}
	RichTextLabel *rich_text_label = memnew(RichTextLabel);
	rich_text_label->set_use_bbcode(true);
	rich_text_label->set_bbcode(about_text);
	rich_text_label->connect("meta_clicked", this, "_on_about_rich_text_label_meta_clicked");

	tab_container->add_child(rich_text_label);

	// License
	RichTextLabel *rich_text_label2 = memnew(RichTextLabel);
	rich_text_label2->set_text(
			"Voxel Tools for Godot Engine\n"
			"-------------------------------\n"
			"Copyright (c) Marc Gilleron\n"
			"\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy of this software and\n"
			"associated documentation files (the \" Software \"), to deal in the Software without restriction,\n"
			"including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,\n"
			"and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,\n"
			"subject to the following conditions:\n"
			"\n"
			"The above copyright notice and this permission notice shall be included in all copies or substantial\n"
			"portions of the Software.\n"
			"\n"
			"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT\n"
			"LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n"
			"IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n"
			"WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n"
			"SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.");
	rich_text_label2->set_selection_enabled(true);

	tab_container->add_child(rich_text_label2);
	tab_container->set_tab_title(0, TTR("About"));
	tab_container->set_tab_title(1, TTR("License"));

	h_box_container->add_child(tab_container);

	v_box_container->add_child(h_box_container);

	HBoxContainer *h_box_container2 = memnew(HBoxContainer);
	h_box_container2->set_alignment(BoxContainer::ALIGN_CENTER);

	Button *button = memnew(Button);
	button->set_text(TTR("Ok"));
	h_box_container2->add_child(button);

	v_box_container->add_child(h_box_container2);

	add_child(v_box_container);
}

void VoxelAboutWindow::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		_icon_texture_rect->set_texture(get_icon("VoxelTerrainLarge", "EditorIcons"));
	}
}

void VoxelAboutWindow::_on_ok_button_pressed() {
	hide();
}

void VoxelAboutWindow::_on_about_rich_text_label_meta_clicked(Variant meta) {
	// Open hyperlinks
	OS::get_singleton()->shell_open(meta);
}

void VoxelAboutWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_ok_button_pressed"), &VoxelAboutWindow::_on_ok_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_about_rich_text_label_meta_clicked", "meta"),
			&VoxelAboutWindow::_on_about_rich_text_label_meta_clicked);
}
