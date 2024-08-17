#ifndef VOXEL_BLOCKY_ATTRIBUTE_H
#define VOXEL_BLOCKY_ATTRIBUTE_H

#include "../../../util/containers/span.h"
#include "../../../util/containers/std_vector.h"
#include "../../../util/godot/classes/resource.h"

namespace zylann::voxel {

// Attribute that can be attached to a VoxelBlockyType.
// It has a unique name and can take two or more positive values. Values can be named.
class VoxelBlockyAttribute : public Resource {
	GDCLASS(VoxelBlockyAttribute, Resource)
public:
	static const int MAX_VALUES = 256;

	virtual StringName get_attribute_name() const;

	// TODO Actually this returns the max value +1. This is not the total amount of values the attribute takes!
	int get_value_count() const;

	int get_default_value() const;
	void set_default_value(int value);

	// TODO -1 means "not found", but I don't like it...
	int get_value_from_name(StringName p_name) const;

	StringName get_name_from_value(int v) const;
	Span<const uint8_t> get_used_values() const;
	bool is_value_used(int v) const;
	// bool has_named_values() const;

	inline bool is_rotation() const {
		return _is_rotation;
	}

	// If the attribute represents a rotation, gets the OrthoBasis index corresponding to that rotation.
	unsigned int get_ortho_rotation_index_from_value(int value) const;

	bool is_equivalent(const VoxelBlockyAttribute &other) const;

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const;
#endif

	static void sort_by_name(Span<Ref<VoxelBlockyAttribute>> attributes);
	static void sort_by_name(Span<StringName> attributes);
	static void sort_by_name(Span<std::pair<StringName, uint8_t>> attributes);

private:
	static void _bind_methods();

protected:
	StringName _name;
	uint8_t _default_value = 0;
	bool _is_rotation = false;
	StdVector<StringName> _value_names;
	StdVector<uint8_t> _ortho_rotations;

	// Attributes don't necessarily use all the values in their range, some can be unused. But they should not take part
	// into the baking process (no "holes").
	StdVector<uint8_t> _used_values;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_ATTRIBUTE_H
