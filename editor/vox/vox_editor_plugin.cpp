#include "vox_editor_plugin.h"
#include "vox_importer.h"

VoxEditorPlugin::VoxEditorPlugin(EditorNode *p_node) {
	Ref<VoxelVoxImporter> vox_importer;
	vox_importer.instance();
	//add_import_plugin(vox_importer);
	ResourceFormatImporter::get_singleton()->add_importer(vox_importer);
}
