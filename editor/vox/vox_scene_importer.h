#ifndef VOX_IMPORTER_H
#define VOX_IMPORTER_H

#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/editor_import_plugin.h"

namespace zylann::voxel::magica {

// Imports a vox file as a scene, where the internal scene layout is preserved as nodes
class VoxelVoxSceneImporter : public zylann::godot::ZN_EditorImportPlugin {
	GDCLASS(VoxelVoxSceneImporter, zylann::godot::ZN_EditorImportPlugin)
public:
	String _zn_get_importer_name() const override;
	String _zn_get_visible_name() const override;
	PackedStringArray _zn_get_recognized_extensions() const override;
	String _zn_get_preset_name(int p_idx) const override;
	int _zn_get_preset_count() const override;
	String _zn_get_save_extension() const override;
	String _zn_get_resource_type() const override;
	double _zn_get_priority() const override;
	int _zn_get_import_order() const override;
	void _zn_get_import_options(StdVector<zylann::godot::ImportOptionWrapper> &p_out_options, const String &p_path,
			int p_preset_index) const override;
	bool _zn_get_option_visibility(const String &p_path, const StringName &p_option_name,
			const zylann::godot::KeyValueWrapper p_options) const override;

	Error _zn_import(const String &p_source_file, const String &p_save_path,
			const zylann::godot::KeyValueWrapper p_options, zylann::godot::StringListWrapper p_out_platform_variants,
			zylann::godot::StringListWrapper p_out_gen_files) const override;

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel::magica

#endif // VOX_IMPORTER_H
