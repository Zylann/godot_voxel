#include "voxel_instance_library_editor_plugin.h"
#include "../../terrain/instancing/voxel_instance_library_multimesh_item.h"
#include "../../terrain/instancing/voxel_instance_library_scene_item.h"
#include "../../util/godot/classes/box_mesh.h"
#include "../../util/godot/classes/confirmation_dialog.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/editor_inspector.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/classes/resource_loader.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"

namespace zylann::voxel {

VoxelInstanceLibraryEditorPlugin::VoxelInstanceLibraryEditorPlugin() {}

// TODO GDX: Can't initialize EditorPlugins in their constructor when they access EditorNode.
// See https://github.com/godotengine/godot-cpp/issues/1179
void VoxelInstanceLibraryEditorPlugin::init() {}

EditorUndoRedoManager &VoxelInstanceLibraryEditorPlugin::get_undo_redo2() {
	EditorUndoRedoManager *ur = get_undo_redo();
	ZN_ASSERT(ur != nullptr);
	return *ur;
}

bool VoxelInstanceLibraryEditorPlugin::_zn_handles(const Object *p_object) const {
	const VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	return lib != nullptr;
}

void VoxelInstanceLibraryEditorPlugin::_zn_edit(Object *p_object) {
	// VoxelInstanceLibrary *lib = Object::cast_to<VoxelInstanceLibrary>(p_object);
	// _library.reference_ptr(lib);
}

void VoxelInstanceLibraryEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		init();

		Control *base_control = get_editor_interface()->get_base_control();
		_inspector_plugin.instantiate();
		_inspector_plugin->plugin = this;
		_inspector_plugin->icon_provider = base_control;
		// TODO Why can other Godot plugins do this in the constructor??
		// I found I could not put this in the constructor,
		// otherwise `add_inspector_plugin` causes ANOTHER editor plugin to leak on exit... Oo
		add_inspector_plugin(_inspector_plugin);

	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_inspector_plugin(_inspector_plugin);
	}
}

void VoxelInstanceLibraryEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
