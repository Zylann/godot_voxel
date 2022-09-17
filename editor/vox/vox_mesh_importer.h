#ifndef VOX_MESH_IMPORTER_H
#define VOX_MESH_IMPORTER_H

#include "../../util/godot/editor_import_plugin.h"
#include "../../util/macros.h"

namespace zylann::voxel::magica {

// Imports a vox file as a single mesh, where all contents of the vox scene is merged
class VoxelVoxMeshImporter : public ZN_EditorImportPlugin {
	GDCLASS(VoxelVoxMeshImporter, ZN_EditorImportPlugin)
protected:
	String _zn_get_importer_name() const override;
	String _zn_get_visible_name() const override;
	PackedStringArray _zn_get_recognized_extensions() const override;
	String _zn_get_preset_name(int p_idx) const override;
	int _zn_get_preset_count() const override;
	String _zn_get_save_extension() const override;
	String _zn_get_resource_type() const override;
	double _zn_get_priority() const override;

	void _zn_get_import_options(
			std::vector<GodotImportOption> &out_options, const String &path, int preset_index) const override;

	bool _zn_get_option_visibility(
			const String &path, const StringName &option_name, const GodotKeyValueWrapper options) const override;

	Error _zn_import(const String &p_source_file, const String &p_save_path, const GodotKeyValueWrapper p_options,
			GodotStringListWrapper out_platform_variants, GodotStringListWrapper out_gen_files) const override;

	enum PivotMode { //
		PIVOT_LOWER_CORNER,
		PIVOT_SCENE_ORIGIN,
		PIVOT_CENTER,
		PIVOT_MODES_COUNT
	};

// TODO GDX: Deriving from a custom class that implements virtual methods fails to compile, unless the derived class
// also implements them... AAAAAAAAAAAAAAAAAAA
#ifdef ZN_GODOT_EXTENSION
public:
	// Re-implementing all those implemented by `ZN_EditorImportPlugin`...
	String _get_importer_name() const override {
		return ZN_EditorImportPlugin::_get_importer_name();
	}
	String _get_visible_name() const override {
		return ZN_EditorImportPlugin::_get_visible_name();
	}
	PackedStringArray _get_recognized_extensions() const override {
		return ZN_EditorImportPlugin::_get_recognized_extensions();
	}
	String _get_preset_name(int64_t p_idx) const override {
		return ZN_EditorImportPlugin::_get_preset_name(p_idx);
	}
	int64_t _get_preset_count() const override {
		return ZN_EditorImportPlugin::_get_preset_count();
	}
	String _get_save_extension() const override {
		return ZN_EditorImportPlugin::_get_save_extension();
	}
	String _get_resource_type() const override {
		return ZN_EditorImportPlugin::_get_resource_type();
	}
	double _get_priority() const override {
		return ZN_EditorImportPlugin::_get_priority();
	}
	Array _get_import_options(const String &path, int64_t preset_index) const override {
		return ZN_EditorImportPlugin::_get_import_options(path, preset_index);
	}
	bool _get_option_visibility(
			const String &path, const StringName &option_name, const Dictionary &options) const override {
		return ZN_EditorImportPlugin::_get_option_visibility(path, option_name, options);
	}
	int64_t _import(const String &source_file, const String &save_path, const Dictionary &options,
			const Array &platform_variants, const Array &gen_files) const override {
		return ZN_EditorImportPlugin::_import(source_file, save_path, options, platform_variants, gen_files);
	}
#endif

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel::magica

#endif // VOX_MESH_IMPORTER_H
