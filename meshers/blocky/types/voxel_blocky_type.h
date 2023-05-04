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
	static const int MAX_VARIANTS = 4096;

	VoxelBlockyType();

	void set_unique_name(StringName name);
	StringName get_unique_name() const;

	void set_base_model(Ref<VoxelBlockyModel> model);
	Ref<VoxelBlockyModel> get_base_model() const;

	Span<const Ref<VoxelBlockyAttribute>> get_attributes() const;
	Ref<VoxelBlockyAttribute> get_attribute_by_name(const StringName &attrib_name) const;

	struct VariantKey {
		// Names must be sorted by string (not StringName pointer comparison).
		// Names and values must be packed at the beginning of arrays. Unused values must be defaulted.
		FixedArray<StringName, MAX_ATTRIBUTES> attribute_names;
		FixedArray<uint8_t, MAX_ATTRIBUTES> attribute_values;

		VariantKey() {
			fill(attribute_values, uint8_t(0));
		}

		bool operator==(const VariantKey &other) const {
			return attribute_names == other.attribute_names && attribute_values == other.attribute_values;
		}
	};

	void bake(std::vector<VoxelBlockyModel::BakedData> &out_models, std::vector<VariantKey> &out_keys,
			VoxelBlockyModel::MaterialIndexer &material_indexer);

	void get_configuration_warnings(PackedStringArray &out_warnings) const;

private:
	static bool parse_variant_key_from_property_name(const String &property_name, VariantKey &out_key);

	// Filters null entries, removes duplicates and sorts attributes before they can be used in processing
	static void gather_and_sort_attributes(const std::vector<Ref<VoxelBlockyAttribute>> &attributes_with_maybe_nulls,
			std::vector<Ref<VoxelBlockyAttribute>> &out_attributes);

	// Generates all combinations from pre-sorted attributes
	static void generate_keys(const std::vector<Ref<VoxelBlockyAttribute>> &attributes,
			std::vector<VariantKey> &out_keys, bool include_rotations);

	void set_variant(const VariantKey &key, Ref<VoxelBlockyModel> model);
	Ref<VoxelBlockyModel> get_variant(const VariantKey &key) const;

	void _on_attribute_changed();

	TypedArray<VoxelBlockyAttribute> _b_get_attributes() const;
	void _b_set_attributes(TypedArray<VoxelBlockyAttribute> attributes);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	StringName _name;
	Ref<VoxelBlockyModel> _base_model;

	// List of unchecked attributes, as specified in the editor. Can have nulls, duplicates, and is not sorted.
	// They are stored this way to allow editing in the Godot editor...
	// TODO Rename `_unchecked_attributes`?
	std::vector<Ref<VoxelBlockyAttribute>> _attributes;

	struct VariantData {
		VariantKey key;
		Ref<VoxelBlockyModel> model;
	};

	// This only contains variants explicitely defined by the user in the editor. Not all runtime variants are here.
	// Can also contain variants that have no relation to any attribute, but these are not saved. They remain in memory
	// to allow the user to go back and forth between configurations in the editor.
	// Saved variants are determined from the combination of current valid attributes.
	std::vector<VariantData> _variants;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_H
