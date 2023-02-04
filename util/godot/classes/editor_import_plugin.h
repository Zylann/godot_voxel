#ifndef ZN_GODOT_EDITOR_IMPORT_PLUGIN_H
#define ZN_GODOT_EDITOR_IMPORT_PLUGIN_H

#if defined(ZN_GODOT)
#include <editor/import/editor_import_plugin.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_import_plugin.hpp>
using namespace godot;
#endif

#include <vector>

namespace zylann {

struct GodotImportOption {
	PropertyInfo option;
	Variant default_value;

	GodotImportOption(PropertyInfo p_option, Variant p_default_value) :
			option(p_option), default_value(p_default_value) {}
};

// Exposes the same interface for different equivalent dictionary types, depending on the compiling target.
struct GodotKeyValueWrapper {
#if defined(ZN_GODOT)

	const HashMap<StringName, Variant> &_map;

	inline bool try_get(const String key, Variant &out_value) const {
		const Variant *vp = _map.getptr(key);
		if (vp != nullptr) {
			out_value = *vp;
			return true;
		}
		return false;
	}

	inline Variant get(const String key) const {
		const Variant *vp = _map.getptr(key);
		if (vp != nullptr) {
			return *vp;
		}
		return Variant();
	}

#elif defined(ZN_GODOT_EXTENSION)

	const Dictionary &_dict;

	inline bool try_get(const String key, Variant &out_value) const {
		if (_dict.has(key)) {
			out_value = _dict[key];
			return true;
		}
		return false;
	}

	inline Variant get(const String key) const {
		return _dict.get(key, Variant());
	}

#endif
};

// Exposes the same interface for different equivalent lists of strings, depending on the compiling target.
struct GodotStringListWrapper {
#if defined(ZN_GODOT)
	List<String> &_list;
	inline void append(const String s) {
		_list.push_back(s);
	}
#elif defined(ZN_GODOT_EXTENSION)
	TypedArray<String> &_array;
	inline void append(const String s) {
		_array.append(s);
	}
#endif
};

// Wraps up differences between compiling as a module and compiling as a GDExtension.
// There are too many annoying differences for this to be done in-place.
class ZN_EditorImportPlugin : public EditorImportPlugin {
	GDCLASS(ZN_EditorImportPlugin, EditorImportPlugin)
public:
#if defined(ZN_GODOT)
	String get_importer_name() const override;
	String get_visible_name() const override;
	void get_recognized_extensions(List<String> *p_extensions) const override;
	String get_preset_name(int p_idx) const override;
	int get_preset_count() const override;
	String get_save_extension() const override;
	String get_resource_type() const override;
	float get_priority() const override;
	// int get_import_order() const override;
	void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	bool get_option_visibility(
			const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	Error import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options,
			List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;

#elif defined(ZN_GODOT_EXTENSION)
	String _get_importer_name() const override;
	String _get_visible_name() const override;
	PackedStringArray _get_recognized_extensions() const override;
	String _get_preset_name(int32_t p_idx) const override;
	int32_t _get_preset_count() const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	double _get_priority() const override;
	TypedArray<Dictionary> _get_import_options(const String &path, int32_t preset_index) const override;
	bool _get_option_visibility(
			const String &path, const StringName &option_name, const Dictionary &options) const override;

	Error _import(const String &source_file, const String &save_path, const Dictionary &options,
			const TypedArray<String> &platform_variants, const TypedArray<String> &gen_files) const override;

#endif

protected:
	// These methods can be implemented once, wrappers above take care of converting.

	virtual String _zn_get_importer_name() const;
	virtual String _zn_get_visible_name() const;
	virtual PackedStringArray _zn_get_recognized_extensions() const;
	virtual String _zn_get_preset_name(int p_idx) const;
	virtual int _zn_get_preset_count() const;
	virtual String _zn_get_save_extension() const;
	virtual String _zn_get_resource_type() const;
	virtual double _zn_get_priority() const;

	virtual void _zn_get_import_options(
			std::vector<GodotImportOption> &p_out_options, const String &p_path, int p_preset_index) const;

	virtual bool _zn_get_option_visibility(
			const String &p_path, const StringName &p_option_name, const GodotKeyValueWrapper p_options) const;

	virtual Error _zn_import(const String &p_source_file, const String &p_save_path,
			const GodotKeyValueWrapper p_options, GodotStringListWrapper p_out_platform_variants,
			GodotStringListWrapper p_out_gen_files) const;

private:
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_GODOT_EDITOR_IMPORT_PLUGIN_H
