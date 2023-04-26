#include "about_window.h"
#include "../util/godot/classes/button.h"
#include "../util/godot/classes/h_box_container.h"
#include "../util/godot/classes/h_split_container.h"
#include "../util/godot/classes/item_list.h"
#include "../util/godot/classes/os.h"
#include "../util/godot/classes/rich_text_label.h"
#include "../util/godot/classes/tab_container.h"
#include "../util/godot/classes/texture_rect.h"
#include "../util/godot/classes/v_box_container.h"
#include "../util/godot/core/array.h"
#include "../util/godot/core/callable.h"
#include "../util/godot/core/string.h"
#include "../util/godot/editor_scale.h"
#include "../util/macros.h"

namespace zylann::voxel {

namespace {
struct ThirdParty {
	const char *name;
	const char *license;
};
const ThirdParty g_third_parties[] = {
	{ "FastNoiseLite",
			"MIT License\n"
			"\n"
			"Copyright(c) 2020 Jordan Peck (jordan.me2@gmail.com)\n"
			"Copyright(c) 2020 Contributors\n"
			"\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
			"of this software and associated documentation files(the \" Software \"), to deal\n"
			"in the Software without restriction, including without limitation the rights\n"
			"to use, copy, modify, merge, publish, distribute, sublicense, and / or sell\n"
			"copies of the Software, and to permit persons to whom the Software is\n"
			"furnished to do so, subject to the following conditions :\n"
			"\n"
			"The above copyright notice and this permission notice shall be included in all\n"
			"copies or substantial portions of the Software.\n"
			"\n"
			"THE SOFTWARE IS PROVIDED \" AS IS \", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
			"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE\n"
			"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
			"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
			"SOFTWARE.\n" },
	{ "FastNoise2",
			"MIT License\n"
			"\n"
			"Copyright (c) 2020 Jordan Peck\n"
			"\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
			"of this software and associated documentation files (the \"Software\"), to deal\n"
			"in the Software without restriction, including without limitation the rights\n"
			"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
			"copies of the Software, and to permit persons to whom the Software is\n"
			"furnished to do so, subject to the following conditions:\n"
			"\n"
			"The above copyright notice and this permission notice shall be included in all\n"
			"copies or substantial portions of the Software.\n"
			"\n"
			"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
			"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
			"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
			"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
			"SOFTWARE.\n" },
	{ "LZ4 compression library",
			"Copyright (c) 2011-2016, Yann Collet\n"
			"All rights reserved.\n"
			"\n"
			"Redistribution and use in source and binary forms, with or without modification,\n"
			"are permitted provided that the following conditions are met:\n"
			"\n"
			"* Redistributions of source code must retain the above copyright notice, this\n"
			"list of conditions and the following disclaimer.\n"
			"\n"
			"* Redistributions in binary form must reproduce the above copyright notice, this\n"
			"list of conditions and the following disclaimer in the documentation and/or\n"
			"other materials provided with the distribution.\n"
			"\n"
			"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \" AS IS \" AND\n"
			"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"
			"WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
			"DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR\n"
			"ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n"
			"(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n"
			"LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON\n"
			"ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
			"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n"
			"SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n" },
	{ "MeshOptimizer",
			"MIT License\n"
			"\n"
			"Copyright (c) 2016-2021 Arseny Kapoulkine\n"
			"\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
			"of this software and associated documentation files (the \"Software\"), to deal\n"
			"in the Software without restriction, including without limitation the rights\n"
			"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
			"copies of the Software, and to permit persons to whom the Software is\n"
			"furnished to do so, subject to the following conditions:\n"
			"\n"
			"The above copyright notice and this permission notice shall be included in all\n"
			"copies or substantial portions of the Software.\n"
			"\n"
			"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
			"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
			"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
			"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
			"SOFTWARE.\n" },
	{ "SQLite",
			"This software is public-domain.\n"
			"The author disclaims copyright to this source code.  In place of\n"
			"a legal notice, here is a blessing:\n"
			"\n"
			"   May you do good and not evil.\n"
			"   May you find forgiveness for yourself and forgive others.\n"
			"   May you share freely, never taking more than you give.\n" },
	{ "Transvoxel tables",
			"Copyright 2009 by Eric Lengyel\n"
			"\n"
			"The following data originates from Eric Lengyel's Transvoxel Algorithm.\n"
			"http://transvoxel.org/\n"
			"\n"
			"The data in this file may be freely used in implementations of the Transvoxel\n"
			"Algorithm. If you do use this data, or any transformation of it, in your own\n"
			"projects, commercial or otherwise, please give credit by indicating in your\n"
			"source code that the data is part of the author's implementation of the\n"
			"Transvoxel Algorithm and that it came from the web address given above.\n"
			"(Simply copying and pasting the two lines of the previous paragraph would be\n"
			"perfect.) If you distribute a commercial product with source code included,\n"
			"then the credit in the source code is required.\n"
			"\n"
			"If you distribute any kind of product that uses this data, a credit visible to\n"
			"the end-user would be appreciated, but it is not required. However, you may\n"
			"not claim that the entire implementation of the Transvoxel Algorithm is your\n"
			"own if you use the data in this file or any transformation of it.\n"
			"\n"
			"The format of the data in this file is described in the dissertation \"Voxel-\n"
			"Based Terrain for Real-Time Virtual Simulations\", available at the web page\n"
			"given above. References to sections and figures below pertain to that paper.\n"
			"\n"
			"The contents of this file are protected by copyright and may not be publicly\n"
			"reproduced without permission.\n" }
};
const unsigned int VOXEL_THIRD_PARTY_COUNT = ZN_ARRAY_LENGTH(g_third_parties);
} // namespace

VoxelAboutWindow::VoxelAboutWindow() {
	// Generated with the help of https://github.com/Zylann/godot_scene_code_converter

	set_title(ZN_TTR("About Voxel Tools"));
	// set_resizable(true); // TODO How to set if a Window is resizable or not?
	set_min_size(Vector2(600, 300) * EDSCALE);

	VBoxContainer *v_box_container = memnew(VBoxContainer);
	v_box_container->set_anchor(SIDE_RIGHT, 1);
	v_box_container->set_anchor(SIDE_BOTTOM, 1);
	v_box_container->set_offset(SIDE_LEFT, 4 * EDSCALE);
	v_box_container->set_offset(SIDE_TOP, 4 * EDSCALE);
	v_box_container->set_offset(SIDE_RIGHT, -4 * EDSCALE);
	v_box_container->set_offset(SIDE_BOTTOM, -4 * EDSCALE);

	// HB
	HBoxContainer *h_box_container = memnew(HBoxContainer);
	h_box_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	TextureRect *texture_rect = memnew(TextureRect);
	// TODO Can't access ANY icon from here because they all return the default empty icon at this stage...
	// texture_rect->set_texture(get_icon("VoxelTerrainLarge", "EditorIcons"));
	texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	_icon_texture_rect = texture_rect;
	h_box_container->add_child(texture_rect);

	TabContainer *tab_container = memnew(TabContainer);
	tab_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	// About
	String about_text = L"[b]Version:[/b] {version}\n"
						"[b]Author:[/b] Marc Gilleron\n"
						"[b]Repository:[/b] [url]https://github.com/Zylann/godot_voxel[/url]\n"
						"[b]Issue tracker:[/b] [url]https://github.com/Zylann/godot_voxel/issues[/url]\n"
						"\n"
						"[b]Gold supporters:[/b]\n"
						"Aaron Franke (aaronfranke)\n"
						"Bewildering\n"
						"\n"
						"[b]Silver supporters:[/b]\n"
						"TheConceptBoy\n"
						"Chris Bolton (yochrisbolton)\n"
						"Gamerfiend (Snowminx)\n"
						"greenlion (Justin Swanhart)\n"
						"segfault-god (jp.owo.Manda)\n"
						"RonanZe\n"
						"Phyronnaz\n"
						"NoFr1ends (Lynx)\n"
						"Kluskey (Jared McCluskey)\n"
						"Trey2k (Trey Moller)\n"
						"marcinn (Marcin Nowak)\n"
						"bfoster68\n"
						"\n"
						"[b]Supporters:[/b]\n"
						"rcorre (Ryan Roden-Corrent)\n"
						"duchainer (Raphaël Duchaîne)\n"
						"MadMartian\n"
						"stackdump (stackdump.eth)\n"
						"Treer\n"
						"MrGreaterThan\n"
						"lenis0012\n"
						"ilievmark (Iliev Mark)\n";
	{
		Dictionary d;
		// TODO Take version from somewhere unique
		d["version"] = "1.x DEV";
		about_text = about_text.format(d);
	}
	RichTextLabel *rich_text_label = memnew(RichTextLabel);
	rich_text_label->set_use_bbcode(true);
	rich_text_label->set_text(about_text);
	rich_text_label->connect(
			"meta_clicked", ZN_GODOT_CALLABLE_MP(this, VoxelAboutWindow, _on_about_rich_text_label_meta_clicked));

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
	tab_container->set_tab_title(0, ZN_TTR("About"));
	tab_container->set_tab_title(1, ZN_TTR("License"));

	// Third-party licenses
	if (VOXEL_THIRD_PARTY_COUNT > 0) {
		HSplitContainer *third_party_container = memnew(HSplitContainer);
		ItemList *third_party_list = memnew(ItemList);
		third_party_list->set_custom_minimum_size(Vector2(200, 0) * EDSCALE);

		for (unsigned int i = 0; i < VOXEL_THIRD_PARTY_COUNT; ++i) {
			const ThirdParty &third_party = g_third_parties[i];
			third_party_list->add_item(third_party.name);
		}

		third_party_list->connect(
				"item_selected", ZN_GODOT_CALLABLE_MP(this, VoxelAboutWindow, _on_third_party_list_item_selected));

		_third_party_rich_text_label = memnew(RichTextLabel);
		_third_party_rich_text_label->set_selection_enabled(true);

		third_party_container->add_child(third_party_list);
		third_party_container->add_child(_third_party_rich_text_label);

		tab_container->add_child(third_party_container);
		tab_container->set_tab_title(2, ZN_TTR("Third party licenses"));

		third_party_list->select(0);
		_on_third_party_list_item_selected(0);
	}

	h_box_container->add_child(tab_container);

	v_box_container->add_child(h_box_container);

	Button *button = get_ok_button();
	button->set_custom_minimum_size(Vector2(100 * EDSCALE, 0));

	add_child(v_box_container);
}

void VoxelAboutWindow::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		_icon_texture_rect->set_texture(get_theme_icon("VoxelTerrainLarge", "EditorIcons"));
	}
}

void VoxelAboutWindow::_on_about_rich_text_label_meta_clicked(Variant meta) {
	// Open hyperlinks
	OS::get_singleton()->shell_open(meta);
}

void VoxelAboutWindow::_on_third_party_list_item_selected(int index) {
	ERR_FAIL_COND(index < 0 || index >= int(VOXEL_THIRD_PARTY_COUNT));
	const ThirdParty &third_party = g_third_parties[index];
	_third_party_rich_text_label->set_text(
			String("{0}\n------------------------------\n{1}").format(varray(third_party.name, third_party.license)));
}

void VoxelAboutWindow::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_about_rich_text_label_meta_clicked", "meta"),
			&VoxelAboutWindow::_on_about_rich_text_label_meta_clicked);
	ClassDB::bind_method(D_METHOD("_on_third_party_list_item_selected", "meta"),
			&VoxelAboutWindow::_on_third_party_list_item_selected);
#endif
}

} // namespace zylann::voxel
