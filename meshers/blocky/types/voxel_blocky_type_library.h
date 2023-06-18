#ifndef VOXEL_BLOCKY_TYPE_LIBRARY_H
#define VOXEL_BLOCKY_TYPE_LIBRARY_H

#include "../voxel_blocky_library_base.h"
#include "voxel_blocky_type.h"

namespace zylann::voxel {

// Library exposing an array of types, for a more high-level system similar to Minecraft blocks. Indices in this array
// don't matter, but names do. Models, rotations and voxel IDs are automatically generated based on attributes of each
// type.
class VoxelBlockyTypeLibrary : public VoxelBlockyLibraryBase {
	GDCLASS(VoxelBlockyTypeLibrary, VoxelBlockyLibraryBase)
public:
	void clear() override;
	void load_default() override;
	void bake() override;
#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

	int get_model_index_default(StringName type_name) const;

	// Shortcut, doesn't require to specify attribute names.
	int get_model_index_single_attribute(StringName type_name, Variant p_attrib_value) const;

	// Gets model index from a type name and the value of all its attributes. Values can be specified
	// either as integers, booleans, or their name if they have one.
	//
	// Example of arguments:
	//
	// (&"mygame:button", {
	//     "direction": VoxelBlockyAttributeDirection.DIR_POSITIVE_Z,
	//     "active": &"on",
	//     "powered": false
	// })
	//
	// Warning: this method is slow. Consider using it in non-intensive code (editing few voxels at a time?), or cache
	// the result in a variable.
	// It is slow because:
	// - Dictionary and each of its keys have to be allocated on the heap
	// - Dictionary keys can't be `StringName` by design (Godot converts them to `String`, but
	//   internally the function has to convert them back to `StringName`)
	// - The function has to iterate the dictionary, which is another thing dictionaries are slow for.
	//   Dictionary is however used for ease of use, and to match the setter function.
	// - Attributes have to be sorted and looked up into internal data structures to obtain the actual voxel ID.
	//
	int get_model_index_with_attributes(StringName type_name, Dictionary attribs_dict) const;

	Ref<VoxelBlockyType> get_type_from_name(StringName p_name) const;

	// Returned array has two elements:
	// - The type's name as StringName
	// - A dictionary where the key is attribute name (unfortunately a String because Godot devs decided to force
	//   converting StringNames to String in Dictionaries) and value is the current integer value of that attribute.
	Array get_type_name_and_attributes_from_model_index(int i) const;

	bool load_id_map_from_string_array(PackedStringArray array);
	PackedStringArray serialize_id_map_to_string_array() const;

	bool load_id_map_from_json(String array);
	String serialize_id_map_to_json() const;

	void get_id_map_preview(PackedStringArray &out_ids, std::vector<uint16_t> &used_ids) const;

private:
	// Fully qualified name identifying a specific model, as a type and the state of each attribute.
	struct VoxelID {
		StringName type_name;
		VoxelBlockyType::VariantKey variant_key;

		bool operator==(const VoxelID &other) const {
			return type_name == other.type_name && variant_key == other.variant_key;
		}
	};

	void update_id_map();
	void update_id_map(std::vector<VoxelID> &id_map, std::vector<uint16_t> *used_ids) const;
	static PackedStringArray serialize_id_map_to_string_array(const std::vector<VoxelID> &id_map);

	static bool parse_voxel_id(const String &str, VoxelID &out_id);
	static String to_string(const VoxelID &id);

	int get_model_index(const VoxelID queried_id) const;

	PackedStringArray _b_get_id_map();
	void _b_set_id_map(PackedStringArray sarray);
	TypedArray<VoxelBlockyType> _b_get_types() const;
	void _b_set_types(TypedArray<VoxelBlockyType> types);
	PackedStringArray _b_serialize_id_map_to_string_array() const;

	static void _bind_methods();

	// Unordered. Can contain nulls.
	std::vector<Ref<VoxelBlockyType>> _types;

	// Maps voxel data indices to fully-qualified model names. This is used to make sure model IDs remain the same, as
	// long as their type has the same name and attribute values are the same.
	// Can refer to types that no longer exist.
	// Indices and size match `_baked_data.models`.
	std::vector<VoxelID> _id_map;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_LIBRARY_H
