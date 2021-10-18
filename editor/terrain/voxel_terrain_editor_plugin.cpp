#include "voxel_terrain_editor_plugin.h"
#include "../../generators/voxel_generator.h"
#include "../../terrain/voxel_lod_terrain.h"
#include "../../terrain/voxel_terrain.h"
#include "../about_window.h"
#include "../graph/voxel_graph_node_inspector_wrapper.h"

#include <editor/editor_scale.h>
#include <scene/3d/camera.h>
#include <scene/gui/menu_button.h>

class VoxelTerrainEditorTaskIndicator : public HBoxContainer {
	GDCLASS(VoxelTerrainEditorTaskIndicator, HBoxContainer)
private:
	enum StatID {
		STAT_STREAM_TASKS,
		STAT_GENERATE_TASKS,
		STAT_MESH_TASKS,
		STAT_MAIN_THREAD_TASKS,
		STAT_MEMORY,
		STAT_COUNT
	};

public:
	VoxelTerrainEditorTaskIndicator() {
		create_stat(STAT_STREAM_TASKS, TTR("Streaming"), TTR("Streaming tasks"));
		create_stat(STAT_GENERATE_TASKS, TTR("Generation"), TTR("Generation tasks"));
		create_stat(STAT_MESH_TASKS, TTR("Meshing"), TTR("Meshing tasks"));
		create_stat(STAT_MAIN_THREAD_TASKS, TTR("Main"), TTR("Main thread tasks"));
		create_stat(STAT_MEMORY, TTR("Memory"), TTR("Memory usage (whole editor, not just voxel)"));
	}

	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_THEME_CHANGED:
				// Set a monospace font.
				// Can't do this in constructor, fonts are not available then. Also the theme can change.
				for (unsigned int i = 0; i < _stats.size(); ++i) {
					_stats[i].label->add_font_override("font", get_font("source", "EditorFonts"));
				}
				break;
		}
	}

	void update_stats() {
		const VoxelServer::Stats stats = VoxelServer::get_singleton()->get_stats();
		set_stat(STAT_STREAM_TASKS, stats.streaming_tasks);
		set_stat(STAT_GENERATE_TASKS, stats.generation_tasks);
		set_stat(STAT_MESH_TASKS, stats.meshing_tasks);
		set_stat(STAT_MAIN_THREAD_TASKS, stats.main_thread_tasks);
		set_stat(STAT_MEMORY, int64_t(OS::get_singleton()->get_static_memory_usage()), "b");
	}

private:
	void create_stat(StatID id, String short_name, String long_name) {
		add_child(memnew(VSeparator));
		Stat &stat = _stats[id];
		CRASH_COND(stat.label != nullptr);
		Label *name_label = memnew(Label);
		name_label->set_text(short_name);
		name_label->set_tooltip(long_name);
		name_label->set_mouse_filter(Control::MOUSE_FILTER_PASS); // Necessary for tooltip to work
		add_child(name_label);
		stat.label = memnew(Label);
		stat.label->set_custom_minimum_size(Vector2(60 * EDSCALE, 0));
		stat.label->set_text("---");
		add_child(stat.label);
	}

	static String with_commas(int64_t n) {
		String res = "";
		if (n < 0) {
			res += "-";
			n = -n;
		}
		String s = String::num_int64(n);
		const int mod = s.length() % 3;
		for (int i = 0; i < s.length(); ++i) {
			if (i != 0 && i % 3 == mod) {
				res += ",";
			}
			res += s[i];
		}
		return res;
	}

	static String with_unit(int64_t n, const char *unit) {
		String s = "";
		if (n < 0) {
			s += "-";
			n = -n;
		}
		if (n >= 1000'000'000) {
			s += with_commas(n / 1000'000'000);
			s += ".";
			s += String::num_int64((n % 1000'000'000) / 1000'000).pad_zeros(3);
			s += " G";
			s += unit;
			return s;
		}
		if (n >= 1000'000) {
			s += String::num_int64(n / 1000'000);
			s += ".";
			s += String::num_int64((n % 1000'000) / 1000).pad_zeros(3);
			s += " M";
			s += unit;
			return s;
		}
		if (n >= 1000) {
			s += String::num_int64(n / 1000);
			s += ".";
			s += String::num_int64(n % 1000).pad_zeros(3);
			s += " K";
			s += unit;
			return s;
		}
		s += String::num_int64(n);
		s += " ";
		s += unit;
		return s;
	}

	void set_stat(StatID id, int64_t value) {
		Stat &stat = _stats[id];
		if (stat.value != value) {
			stat.value = value;
			stat.label->set_text(with_commas(stat.value));
		}
	}

	void set_stat(StatID id, int64_t value, const char *unit) {
		Stat &stat = _stats[id];
		if (stat.value != value) {
			stat.value = value;
			stat.label->set_text(with_unit(stat.value, unit));
		}
	}

	struct Stat {
		int64_t value = 0;
		Label *label = nullptr;
	};

	FixedArray<Stat, STAT_COUNT> _stats;
};

VoxelTerrainEditorPlugin::VoxelTerrainEditorPlugin(EditorNode *p_node) {
	MenuButton *menu_button = memnew(MenuButton);
	menu_button->set_text(TTR("Terrain"));
	menu_button->get_popup()->add_item(TTR("Re-generate"), MENU_RESTART_STREAM);
	menu_button->get_popup()->add_item(TTR("Re-mesh"), MENU_REMESH);
	menu_button->get_popup()->add_separator();
	menu_button->get_popup()->add_item(TTR("Stream follow camera"), MENU_STREAM_FOLLOW_CAMERA);
	{
		const int i = menu_button->get_popup()->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
		menu_button->get_popup()->set_item_as_checkable(i, true);
		menu_button->get_popup()->set_item_checked(i, _editor_viewer_follows_camera);
	}
	menu_button->get_popup()->add_separator();
	menu_button->get_popup()->add_item(TTR("About Voxel Tools..."), MENU_ABOUT);
	menu_button->get_popup()->connect("id_pressed", this, "_on_menu_item_selected");
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;

	_task_indicator = memnew(VoxelTerrainEditorTaskIndicator);
	_task_indicator->hide();
	add_control_to_container(EditorPlugin::CONTAINER_SPATIAL_EDITOR_BOTTOM, _task_indicator);

	Node *base_control = get_editor_interface()->get_base_control();

	_about_window = memnew(VoxelAboutWindow);
	base_control->add_child(_about_window);
}

void VoxelTerrainEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			_editor_viewer_id = VoxelServer::get_singleton()->add_viewer();
			VoxelServer::get_singleton()->set_viewer_distance(_editor_viewer_id, 512);
			// No collision needed in editor, also it updates faster without
			VoxelServer::get_singleton()->set_viewer_requires_collisions(_editor_viewer_id, false);
			break;

		case NOTIFICATION_EXIT_TREE:
			VoxelServer::get_singleton()->remove_viewer(_editor_viewer_id);
			break;

		case NOTIFICATION_PROCESS:
			_task_indicator->update_stats();
			break;
	}
}

// Things the plugin doesn't directly work on, but still handles to keep things visible.
// This is basically a hack because it's not easy to express that with EditorPlugin API.
// The use case being, as long as we edit an object NESTED within a voxel terrain, we should keep things visible.
static bool is_side_handled(Object *p_object) {
	// Handle stream too so we can leave some controls visible while we edit a stream or generator
	VoxelGenerator *generator = Object::cast_to<VoxelGenerator>(p_object);
	if (generator != nullptr) {
		return true;
	}
	// And have to account for this hack as well
	VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	return false;
}

bool VoxelTerrainEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<VoxelNode>(p_object) != nullptr) {
		return true;
	}
	if (_node != nullptr) {
		return is_side_handled(p_object);
	}
	return false;
}

void VoxelTerrainEditorPlugin::edit(Object *p_object) {
	VoxelNode *node = Object::cast_to<VoxelNode>(p_object);
	if (node != nullptr) {
		set_node(node);
	} else {
		if (!is_side_handled(p_object)) {
			set_node(nullptr);
		}
	}
}

void VoxelTerrainEditorPlugin::set_node(VoxelNode *node) {
	if (_node != nullptr) {
		// Using this to know when the node becomes really invalid, because ObjectID is unreliable in Godot 3.x,
		// and we may want to keep access to the node when we select some different kinds of objects.
		// Also moving the node around in the tree triggers exit/enter so have to listen for both.
		_node->disconnect("tree_entered", this, "_on_terrain_tree_entered");
		_node->disconnect("tree_exited", this, "_on_terrain_tree_exited");

		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);
		if (vlt != nullptr) {
			vlt->set_show_gizmos(false);
		}
	}

	_node = node;

	if (_node != nullptr) {
		_node->connect("tree_entered", this, "_on_terrain_tree_entered", varray(_node));
		_node->connect("tree_exited", this, "_on_terrain_tree_exited", varray(_node));

		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);
		if (vlt != nullptr) {
			vlt->set_show_gizmos(true);
		}
	}
}

void VoxelTerrainEditorPlugin::make_visible(bool visible) {
	_menu_button->set_visible(visible);
	_task_indicator->set_visible(visible);
	set_process(visible);

	if (_node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);
		if (vlt != nullptr) {
			vlt->set_show_gizmos(visible);
		}
	}

	// TODO There are deselection problems I cannot fix cleanly!

	// Can't use `make_visible(false)` to reset our reference to the node or reset gizmos,
	// because of https://github.com/godotengine/godot/issues/40166
	// So we'll need to check if _node is null all over the place
}

bool VoxelTerrainEditorPlugin::forward_spatial_gui_input(Camera *p_camera, const Ref<InputEvent> &p_event) {
	VoxelServer::get_singleton()->set_viewer_distance(_editor_viewer_id, p_camera->get_zfar());
	_editor_camera_last_position = p_camera->get_global_transform().origin;

	if (_editor_viewer_follows_camera) {
		VoxelServer::get_singleton()->set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
	}

	return false;
}

void VoxelTerrainEditorPlugin::_on_menu_item_selected(int id) {
	switch (id) {
		case MENU_RESTART_STREAM:
			ERR_FAIL_COND(_node == nullptr);
			_node->restart_stream();
			break;

		case MENU_REMESH:
			ERR_FAIL_COND(_node == nullptr);
			_node->remesh_all_blocks();
			break;

		case MENU_STREAM_FOLLOW_CAMERA: {
			_editor_viewer_follows_camera = !_editor_viewer_follows_camera;

			const int i = _menu_button->get_popup()->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
			_menu_button->get_popup()->set_item_checked(i, _editor_viewer_follows_camera);

			if (_editor_viewer_follows_camera) {
				VoxelServer::get_singleton()->set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
			}
		} break;

		case MENU_ABOUT:
			_about_window->popup_centered_ratio(0.6);
			break;
	}
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_entered(Node *node) {
	_node = Object::cast_to<VoxelNode>(node);
	ERR_FAIL_COND(_node == nullptr);
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_exited(Node *node) {
	// If the node exited the tree because it was deleted, signals we connected should automatically disconnect.
	_node = nullptr;
}

void VoxelTerrainEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_menu_item_selected", "id"), &VoxelTerrainEditorPlugin::_on_menu_item_selected);
	ClassDB::bind_method(D_METHOD("_on_terrain_tree_entered"), &VoxelTerrainEditorPlugin::_on_terrain_tree_entered);
	ClassDB::bind_method(D_METHOD("_on_terrain_tree_exited"), &VoxelTerrainEditorPlugin::_on_terrain_tree_exited);
}
