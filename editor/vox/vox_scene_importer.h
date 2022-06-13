#ifndef VOX_IMPORTER_H
#define VOX_IMPORTER_H

#include <editor/import/editor_import_plugin.h>

namespace zylann::voxel::magica {

// Imports a vox file as a scene, where the internal scene layout is preserved as nodes
class VoxelVoxSceneImporter : public ResourceImporter {
	GDCLASS(VoxelVoxSceneImporter, ResourceImporter)
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
	void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	bool get_option_visibility(
			const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	Error import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options,
			List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};

} // namespace zylann::voxel::magica

#endif // VOX_IMPORTER_H
