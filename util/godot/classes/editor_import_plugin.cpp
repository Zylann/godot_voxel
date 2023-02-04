#include "editor_import_plugin.h"
#include "../../errors.h"

namespace zylann {

#if defined(ZN_GODOT)

String ZN_EditorImportPlugin::get_importer_name() const {
	return _zn_get_importer_name();
}

String ZN_EditorImportPlugin::get_visible_name() const {
	return _zn_get_visible_name();
}

void ZN_EditorImportPlugin::get_recognized_extensions(List<String> *p_extensions) const {
	ZN_ASSERT(p_extensions != nullptr);
	const PackedStringArray extensions = _zn_get_recognized_extensions();
	for (const String &extension : extensions) {
		p_extensions->push_back(extension);
	}
}

String ZN_EditorImportPlugin::get_preset_name(int p_idx) const {
	return _zn_get_preset_name(p_idx);
}

int ZN_EditorImportPlugin::get_preset_count() const {
	return _zn_get_preset_count();
}

String ZN_EditorImportPlugin::get_save_extension() const {
	return _zn_get_save_extension();
}

String ZN_EditorImportPlugin::get_resource_type() const {
	return _zn_get_resource_type();
}

float ZN_EditorImportPlugin::get_priority() const {
	return _zn_get_priority();
}

void ZN_EditorImportPlugin::get_import_options(
		const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	ZN_ASSERT(r_options != nullptr);
	std::vector<GodotImportOption> options;
	_zn_get_import_options(options, p_path, p_preset);
	for (const GodotImportOption &option : options) {
		ImportOption opt(option.option, option.default_value);
		r_options->push_back(opt);
	}
}

bool ZN_EditorImportPlugin::get_option_visibility(
		const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return _zn_get_option_visibility(p_path, p_option, GodotKeyValueWrapper{ p_options });
}

Error ZN_EditorImportPlugin::import(const String &p_source_file, const String &p_save_path,
		const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
		Variant *r_metadata) {
	ZN_ASSERT(r_platform_variants != nullptr);
	ZN_ASSERT(r_gen_files != nullptr);
	return _zn_import(p_source_file, p_save_path, GodotKeyValueWrapper{ p_options },
			GodotStringListWrapper{ *r_platform_variants }, GodotStringListWrapper{ *r_gen_files });
}

#elif defined(ZN_GODOT_EXTENSION)

String ZN_EditorImportPlugin::_get_importer_name() const {
	return _zn_get_importer_name();
}

String ZN_EditorImportPlugin::_get_visible_name() const {
	return _zn_get_visible_name();
}

PackedStringArray ZN_EditorImportPlugin::_get_recognized_extensions() const {
	return _zn_get_recognized_extensions();
}

String ZN_EditorImportPlugin::_get_preset_name(int32_t p_idx) const {
	return _zn_get_preset_name(p_idx);
}

int32_t ZN_EditorImportPlugin::_get_preset_count() const {
	return _zn_get_preset_count();
}

String ZN_EditorImportPlugin::_get_save_extension() const {
	return _zn_get_save_extension();
}

String ZN_EditorImportPlugin::_get_resource_type() const {
	return _zn_get_resource_type();
}

double ZN_EditorImportPlugin::_get_priority() const {
	return _zn_get_priority();
}

TypedArray<Dictionary> ZN_EditorImportPlugin::_get_import_options(const String &path, int32_t preset_index) const {
	std::vector<GodotImportOption> options;
	_zn_get_import_options(options, path, preset_index);

	TypedArray<Dictionary> output;

	const String name_key = "name";
	const String property_hint_key = "property_hint";
	const String default_value_key = "default_value";
	const String hint_string_key = "hint_string";
	const String usage_key = "usage";

	for (const GodotImportOption option : options) {
		Dictionary d;
		d[name_key] = String(option.option.name);
		d[default_value_key] = option.default_value;
		d[property_hint_key] = option.option.hint;
		d[hint_string_key] = String(option.option.hint_string);
		d[usage_key] = option.option.usage;
	}

	return output;
}

bool ZN_EditorImportPlugin::_get_option_visibility(
		const String &path, const StringName &option_name, const Dictionary &options) const {
	return _zn_get_option_visibility(path, option_name, GodotKeyValueWrapper{ options });
}

Error ZN_EditorImportPlugin::_import(const String &source_file, const String &save_path, const Dictionary &options,
		const TypedArray<String> &platform_variants, const TypedArray<String> &gen_files) const {
	// TODO GDX: `EditorImportPlugin::_import` is passing constant arrays for parameters that should be writable
	TypedArray<String> &platform_variants_writable = const_cast<TypedArray<String> &>(platform_variants);
	TypedArray<String> &gen_files_writable = const_cast<TypedArray<String> &>(gen_files);
	return _zn_import(source_file, save_path, GodotKeyValueWrapper{ options },
			GodotStringListWrapper{ platform_variants_writable }, GodotStringListWrapper{ gen_files_writable });
}

#endif

String ZN_EditorImportPlugin::_zn_get_importer_name() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return "<unnamed>";
}

String ZN_EditorImportPlugin::_zn_get_visible_name() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return "<unnamed>";
}
PackedStringArray ZN_EditorImportPlugin::_zn_get_recognized_extensions() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return PackedStringArray();
}

String ZN_EditorImportPlugin::_zn_get_preset_name(int p_idx) const {
	ZN_PRINT_ERROR("Method is not implemented");
	return "<unnamed>";
}

int ZN_EditorImportPlugin::_zn_get_preset_count() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return 0;
}

String ZN_EditorImportPlugin::_zn_get_save_extension() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return "";
}

String ZN_EditorImportPlugin::_zn_get_resource_type() const {
	ZN_PRINT_ERROR("Method is not implemented");
	return "";
}

double ZN_EditorImportPlugin::_zn_get_priority() const {
	return 1.0;
}

void ZN_EditorImportPlugin::_zn_get_import_options(
		std::vector<GodotImportOption> &p_out_options, const String &p_path, int p_preset_index) const {
	ZN_PRINT_ERROR("Method is not implemented");
}

bool ZN_EditorImportPlugin::_zn_get_option_visibility(
		const String &p_path, const StringName &p_option_name, const GodotKeyValueWrapper p_options) const {
	ZN_PRINT_ERROR("Method is not implemented");
	return false;
}

Error ZN_EditorImportPlugin::_zn_import(const String &p_source_file, const String &p_save_path,
		const GodotKeyValueWrapper p_options, GodotStringListWrapper p_out_platform_variants,
		GodotStringListWrapper p_out_gen_files) const {
	ZN_PRINT_ERROR("Method is not implemented");
	return ERR_METHOD_NOT_FOUND;
}

} // namespace zylann
