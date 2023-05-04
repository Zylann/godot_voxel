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
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;

	int get_model_index_default(StringName type_name) const;

	// Shortcut, doesn't require to specify attribute names.
	int get_model_index_single_attribute(StringName type_name, Variant p_attrib_value) const;

	// TODO For ease of use, this would have to accept attributes in the same format
	// `get_type_and_attributes_from_model_index` returns them... which is a disaster for performance, so perhaps they
	// should be seen as slow shortcuts, and a helper object could be used in other cases?
	//
	// (&"mygame:button", {
	//     "direction": VoxelBlockyAttributeDirection.DIR_POSITIVE_Z,
	//     "active": &"on",
	//     "powered": false
	// })
	//
	// Gets model index from a type name and an unordered list of attributes names and values. Values can be specified
	// either as raw integers, or their name if they have one.
	//
	// Example of arguments:
	//
	// (&"mygame:button", [
	//     &"direction", VoxelBlockyAttributeDirection.DIR_POSITIVE_Z,
	//     &"active", &"on",
	//     &"powered", false
	// ])
	//
	int get_model_index_with_attributes(StringName type_name, Array attribs) const;

	Ref<VoxelBlockyType> get_type_from_name(StringName name) const;
	// Ref<VoxelBlockyType> get_type_from_model_index(int i) const;

	// TODO The script API is TERRIBLE at this. It will work, but really can't call that millions of times.
	// Maybe there could be an alternative API using a "sampler" object, so a more efficient data structure could be
	// used instead of allocating arrays and dictionaries on each call?
	//
	// Returned array has two elements:
	// - The type
	// - A dictionary where the key is attribute name (unfortunately a string because Godot devs decided to force
	//   converting StringNames to String in Dictionaries) and value is the current value of that attribute.
	// Array get_type_and_attributes_from_model_index(int i) const;

private:
	struct VoxelID {
		StringName type_name;
		VoxelBlockyType::VariantKey variant_key;

		bool operator==(const VoxelID &other) const {
			return type_name == other.type_name && variant_key == other.variant_key;
		}
	};

	int get_model_index(const VoxelID queried_id) const;

	static void _bind_methods();

	TypedArray<VoxelBlockyType> _b_get_types() const;
	void _b_set_types(TypedArray<VoxelBlockyType> types);

	// Unordered.
	std::vector<Ref<VoxelBlockyType>> _types;

	// Maps voxel data indices to fully-qualified model names.
	// Can refer to types that no longer exist.
	std::vector<VoxelID> _id_map;
};

// class VoxelBlockyTypeHelper : public RefCounted {
// 	GDCLASS(VoxelBlockyTypeHelper, RefCounted)
// public:
// 	int get_model_index() const;
// 	void set_model_index(int i);

// 	void has_attribute(StringName name);

// 	int get_attribute_value(StringName name);
// 	StringName get_attribute_value_name(StringName name);
// 	void set_attribute_value(StringName name, Variant value);

// 	Dictionary get_attributes_dict();
// 	void set_attributes_dict(Dictionary dict);

// void rotate_90(Vector3i::Axis axis, bool clockwise);

// private:
// 	int _model_index;
// 	Ref<VoxelBlockyTypeLibrary> _library;
// };

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_LIBRARY_H
