#include "vox_editor_plugin.h"
#include "vox_mesh_importer.h"
#include "vox_scene_importer.h"

namespace zylann::voxel::magica {

VoxEditorPlugin::VoxEditorPlugin() {
	Ref<VoxelVoxSceneImporter> vox_scene_importer;
	vox_scene_importer.instantiate();
	//add_import_plugin(vox_importer);
	ResourceFormatImporter::get_singleton()->add_importer(vox_scene_importer);

	Ref<VoxelVoxMeshImporter> vox_mesh_importer;
	vox_mesh_importer.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(vox_mesh_importer);
}

} // namespace zylann::voxel::magica
