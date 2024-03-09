#ifndef VOX_MESH_IMPORTER_H
#define VOX_MESH_IMPORTER_H

#include "../../util/godot/classes/editor_import_plugin.h"
#include "../../util/macros.h"

namespace zylann::voxel::magica {

// Imports a vox file as a single mesh, where all contents of the vox scene is merged
class VoxelVoxMeshImporter : public zylann::godot::ZN_EditorImportPlugin {
	GDCLASS(VoxelVoxMeshImporter, zylann::godot::ZN_EditorImportPlugin)
protected:
	String _zn_get_importer_name() const override;
	String _zn_get_visible_name() const override;
	PackedStringArray _zn_get_recognized_extensions() const override;
	String _zn_get_preset_name(int p_idx) const override;
	int _zn_get_preset_count() const override;
	String _zn_get_save_extension() const override;
	String _zn_get_resource_type() const override;
	double _zn_get_priority() const override;

	void _zn_get_import_options(StdVector<zylann::godot::ImportOptionWrapper> &out_options, const String &path,
			int preset_index) const override;

	bool _zn_get_option_visibility(const String &path, const StringName &option_name,
			const zylann::godot::KeyValueWrapper options) const override;

	Error _zn_import(const String &p_source_file, const String &p_save_path,
			const zylann::godot::KeyValueWrapper p_options, zylann::godot::StringListWrapper out_platform_variants,
			zylann::godot::StringListWrapper out_gen_files) const override;

	enum PivotMode { //
		PIVOT_LOWER_CORNER,
		PIVOT_SCENE_ORIGIN,
		PIVOT_CENTER,
		PIVOT_MODES_COUNT
	};

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel::magica

#endif // VOX_MESH_IMPORTER_H
