#include "vox_editor_plugin.h"
#include "../../util/godot/classes/node.h"

namespace zylann::voxel::magica {

VoxelVoxEditorPlugin::VoxelVoxEditorPlugin() {}

void VoxelVoxEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_vox_scene_importer.instantiate();
			add_import_plugin(_vox_scene_importer);
			// ResourceFormatImporter::get_singleton()->add_importer(vox_scene_importer);

			_vox_mesh_importer.instantiate();
			add_import_plugin(_vox_mesh_importer);
			// ResourceFormatImporter::get_singleton()->add_importer(vox_mesh_importer);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_import_plugin(_vox_scene_importer);
			remove_import_plugin(_vox_mesh_importer);
		} break;
	}
}

} // namespace zylann::voxel::magica
