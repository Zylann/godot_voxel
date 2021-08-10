#include "vox_editor_plugin.h"
#include "vox_importer.h"
#include "vox_mesh_importer.h"

VoxEditorPlugin::VoxEditorPlugin(EditorNode *p_node) {
	Ref<VoxelVoxImporter> vox_scene_importer;
	vox_scene_importer.instance();
	//add_import_plugin(vox_importer);
	ResourceFormatImporter::get_singleton()->add_importer(vox_scene_importer);

	Ref<VoxelVoxMeshImporter> vox_mesh_importer;
	vox_mesh_importer.instance();
	ResourceFormatImporter::get_singleton()->add_importer(vox_mesh_importer);
}
