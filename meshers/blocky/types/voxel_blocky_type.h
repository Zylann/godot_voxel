#ifndef VOXEL_BLOCKY_TYPE_H
#define VOXEL_BLOCKY_TYPE_H

#include "../../../util/godot/classes/resource.h"
#include "../voxel_blocky_model.h"
#include "voxel_blocky_attribute.h"
#include <vector>

namespace zylann::voxel {

// High-level representation of a "type" of voxel for use with `VoxelMesherBlocky` and `VoxelBlockyTypeLibrary`.
// One type can represent multiple possible values in voxel data, corresponding to states of that type (such as
// rotation, connections, on/off etc).
class VoxelBlockyType : public Resource {
	GDCLASS(VoxelBlockyType, Resource)
public:
	// Should be kept as low as possible. Increase if needed.
	static const int MAX_ATTRIBUTES = 4;
	// Maximum amount of variants that can be edited as a list of combinations.
	// Technical limit is 65536, but reaching hundreds indicates a design problem, and can be overwhelming to edit. Such
	// amount of variants is possible though, in which case we should implement conditionals.
	static const int MAX_EDITING_VARIANTS = 256;

	VoxelBlockyType();

	void set_unique_name(StringName p_name);
	StringName get_unique_name() const;

	void set_base_model(Ref<VoxelBlockyModel> model);
	Ref<VoxelBlockyModel> get_base_model() const;

	Span<const Ref<VoxelBlockyAttribute>> get_attributes() const;
	Ref<VoxelBlockyAttribute> get_attribute_by_name(const StringName &attrib_name) const;
	Ref<VoxelBlockyAttribute> get_rotation_attribute() const;

	void get_checked_attributes(std::vector<Ref<VoxelBlockyAttribute>> &out_attribs) const;

	// Identifies one model variant of a type, as the attributes and values it has.
	struct VariantKey {
		// Names must be sorted by string (not StringName pointer comparison).
		// In the design, this is not really required, user and config files can specify attributes in any order,
		// but we do this at runtime to improve performance when looking them up.
		// Names and values must be packed at the beginning of arrays. Unused values must be defaulted.
		FixedArray<StringName, MAX_ATTRIBUTES> attribute_names;
		FixedArray<uint8_t, MAX_ATTRIBUTES> attribute_values;

		VariantKey() {
			fill(attribute_values, uint8_t(0));
		}

		bool operator==(const VariantKey &other) const {
			return attribute_names == other.attribute_names && attribute_values == other.attribute_values;
		}

		String to_string() const;
		String to_string(Span<const Ref<VoxelBlockyAttribute>> context_attributes) const;
		bool parse_from_array(const Array &array);
		Array to_array() const;
	};

	// Get or set models specifically associated to a particular variant (they are not necessarily the final result)
	void set_variant(const VariantKey &key, Ref<VoxelBlockyModel> model);
	Ref<VoxelBlockyModel> get_variant(const VariantKey &key) const;

	void bake(std::vector<VoxelBlockyModel::BakedData> &out_models, std::vector<VariantKey> &out_keys,
			VoxelBlockyModel::MaterialIndexer &material_indexer, const VariantKey *specific_key) const;

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const;
#endif

	Ref<Mesh> get_preview_mesh(const VariantKey &key) const;

	void generate_keys(std::vector<VariantKey> &out_keys, bool include_rotations) const;

private:
	// Filters null entries, removes duplicates and sorts attributes before they can be used in processing
	static void gather_and_sort_attributes(const std::vector<Ref<VoxelBlockyAttribute>> &attributes_with_maybe_nulls,
			std::vector<Ref<VoxelBlockyAttribute>> &out_attributes);

	// Generates all combinations from pre-sorted attributes.
	static void generate_keys(const std::vector<Ref<VoxelBlockyAttribute>> &attributes,
			std::vector<VariantKey> &out_keys, bool include_rotations);

	void _on_attribute_changed();
	void _on_base_model_changed();

	TypedArray<VoxelBlockyAttribute> _b_get_attributes() const;
	void _b_set_attributes(TypedArray<VoxelBlockyAttribute> attributes);

	// bool _set(const StringName &p_name, const Variant &p_value);
	// bool _get(const StringName &p_name, Variant &r_ret) const;
	// void _get_property_list(List<PropertyInfo> *p_list) const;

	void _b_set_variant_model(Array p_key, Ref<VoxelBlockyModel> model);
	void _b_set_variant_models_data(Array data);
	Array _b_get_variant_models_data() const;

	static void _bind_methods();

	// Name of the type, as used in development, config files, save files or commands. It must be unique, and maybe
	// prefixed if your game supports modding. To display it in-game, it may be preferable to use a translation
	// dictionary rather than using it directly.
	StringName _name;

	Ref<VoxelBlockyModel> _base_model;

	// List of unchecked attributes, as specified in the editor. Can have nulls, duplicates, and is not sorted.
	// They are stored this way to allow editing in the Godot editor...
	// TODO Rename `_unchecked_attributes`?
	std::vector<Ref<VoxelBlockyAttribute>> _attributes;

	// TODO Automatic rotation isn't always possible.
	// For example, if a 3-axis block is asymetric and needs to always point down in its Y axis configuration, there is
	// no way to choose that (other than wasting a 6-dir attribute)

	// If true, rotation attributes will not require the user to specify models for each rotation. They will be
	// automatically generated, using the default rotation as reference.
	bool _automatic_rotations = true;

	struct VariantData {
		VariantKey key;
		Ref<VoxelBlockyModel> model;
	};

	// This only contains variants explicitely defined by the user in the editor. Not all runtime variants are here.
	// Can also contain variants that have no relation to any attribute, but these are not saved. They remain in memory
	// to allow the user to go back and forth between configurations in the editor as they make changes.
	// Saved variants are determined from the combination of current valid attributes.
	std::vector<VariantData> _variants;

	// TODO Conditional models
};

String to_string(const VoxelBlockyType::VariantKey &key);

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_H
