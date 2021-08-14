#ifndef VOX_MESH_IMPORTER_H
#define VOX_MESH_IMPORTER_H

#include <editor/import/editor_import_plugin.h>

// Imports a vox file as a single mesh, where all contents of the vox scene is merged
class VoxelVoxMeshImporter : public ResourceImporter {
	GDCLASS(VoxelVoxMeshImporter, ResourceImporter)
public:
	String get_importer_name() const override;
	String get_visible_name() const override;
	void get_recognized_extensions(List<String> *p_extensions) const override;
	String get_preset_name(int p_idx) const override;
	int get_preset_count() const override;
	String get_save_extension() const override;
	String get_resource_type() const override;
	float get_priority() const override;
	//int get_import_order() const override;
	void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const override;
	bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const override;

	Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options,
			List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;

	enum PivotMode {
		PIVOT_LOWER_CORNER,
		PIVOT_SCENE_ORIGIN,
		PIVOT_CENTER,
		PIVOT_MODES_COUNT
	};
};

#endif // VOX_MESH_IMPORTER_H
