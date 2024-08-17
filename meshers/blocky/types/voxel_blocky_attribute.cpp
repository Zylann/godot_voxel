#include "voxel_blocky_attribute.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/containers/container_funcs.h"
#include "../../../util/godot/core/array.h"
#include "../../../util/io/log.h"
#include "../../../util/math/funcs.h"
#include "../../../util/math/ortho_basis.h"

namespace zylann::voxel {

StringName VoxelBlockyAttribute::get_attribute_name() const {
	return _name;
}

int VoxelBlockyAttribute::get_value_count() const {
	return _value_names.size();
}

int VoxelBlockyAttribute::get_default_value() const {
	return _default_value;
}

void VoxelBlockyAttribute::set_default_value(int value) {
	// TODO Can't validate that the value is in bounds, because we can't guarantee the order in which Godot will call
	// setters of the resource when loading it
	_default_value = math::max(value, 0);
}

int VoxelBlockyAttribute::get_value_from_name(StringName p_name) const {
	for (unsigned int i = 0; i < _value_names.size(); ++i) {
		if (_value_names[i] == p_name) {
			return i;
		}
	}
	return -1;
}

StringName VoxelBlockyAttribute::get_name_from_value(int v) const {
	ZN_ASSERT_RETURN_V(v >= 0 && v < get_value_count(), StringName());
	return _value_names[v];
}

Span<const uint8_t> VoxelBlockyAttribute::get_used_values() const {
	return to_span(_used_values);
}

bool VoxelBlockyAttribute::is_value_used(int v) const {
	for (const uint8_t used_value : _used_values) {
		if (v == used_value) {
			return true;
		}
	}
	return false;
}

bool VoxelBlockyAttribute::is_equivalent(const VoxelBlockyAttribute &other) const {
	if (_name != other._name) {
		return false;
	}
	Span<const uint8_t> self_used_values = get_used_values();
	Span<const uint8_t> other_used_values = other.get_used_values();
	if (self_used_values.size() != other_used_values.size()) {
		return false;
	}
	for (unsigned int i = 0; i < self_used_values.size(); ++i) {
		if (self_used_values[i] != other_used_values[i]) {
			return false;
		}
	}
	for (unsigned int i = 0; i < self_used_values.size(); ++i) {
		const String value_name = get_name_from_value(self_used_values[i]);
		const String other_value_name = other.get_name_from_value(other_used_values[i]);
		if (value_name != other_value_name) {
			return false;
		}
	}
	return true;
}

bool find_non_empty_duplicate(const StdVector<StringName> &names) {
	for (unsigned int i = 0; i < names.size(); ++i) {
		const StringName &a = names[i];
		if (a == StringName()) {
			continue;
		}
		for (unsigned int j = i + 1; j < names.size(); ++j) {
			const StringName &b = names[j];
			if (b == StringName()) {
				continue;
			}
			if (a == b) {
				return true;
			}
		}
	}
	return false;
}

#ifdef TOOLS_ENABLED

void VoxelBlockyAttribute::get_configuration_warnings(PackedStringArray &out_warnings) const {
	if (find_non_empty_duplicate(_value_names)) {
		out_warnings.push_back(String("{0} named {1} has multiple values with the same name.")
									   .format(varray(get_class(), get_attribute_name())));
	}

	if (!contains(to_span(_used_values), _default_value)) {
		out_warnings.push_back(String("{0} named {1} has an invalid default value.")
									   .format(varray(get_class(), get_attribute_name())));
	}
}

#endif

// int VoxelBlockyAttribute::get_order() const {
// 	ZN_PRINT_ERROR("Not implemented");
// 	// Implemented in child classes
// 	return 0;
// }

unsigned int VoxelBlockyAttribute::get_ortho_rotation_index_from_value(int value) const {
	ZN_ASSERT_RETURN_V(value >= 0 && value < int(_value_names.size()), math::ORTHO_ROTATION_IDENTITY);
	if (value >= int(_ortho_rotations.size())) {
		return math::ORTHO_ROTATION_IDENTITY;
	}
	return _ortho_rotations[value];
}

void VoxelBlockyAttribute::sort_by_name(Span<Ref<VoxelBlockyAttribute>> attributes) {
	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.is_valid());
	}
	struct AttributeComparator {
		bool operator()(const Ref<VoxelBlockyAttribute> &a, const Ref<VoxelBlockyAttribute> &b) const {
			return String(a->get_attribute_name()) < String(b->get_attribute_name());
		}
	};
	SortArray<Ref<VoxelBlockyAttribute>, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::sort_by_name(Span<StringName> attributes) {
	for (const StringName &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib != StringName());
	}
	struct AttributeComparator {
		bool operator()(const StringName &a, const StringName &b) const {
			return String(a) < String(b);
		}
	};
	SortArray<StringName, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::sort_by_name(Span<std::pair<StringName, uint8_t>> attributes) {
	for (const std::pair<StringName, uint8_t> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.first != StringName());
	}
	struct AttributeComparator {
		bool operator()(const std::pair<StringName, uint8_t> &a, const std::pair<StringName, uint8_t> &b) const {
			return String(a.first) < String(b.first);
		}
	};
	SortArray<std::pair<StringName, uint8_t>, AttributeComparator> sorter;
	sorter.sort(attributes.data(), attributes.size());
}

void VoxelBlockyAttribute::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_attribute_name"), &VoxelBlockyAttribute::get_attribute_name);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VoxelBlockyAttribute::get_default_value);
	ClassDB::bind_method(D_METHOD("get_value_count"), &VoxelBlockyAttribute::get_value_count);
	ClassDB::bind_method(D_METHOD("is_rotation"), &VoxelBlockyAttribute::is_rotation);

	BIND_CONSTANT(MAX_VALUES);
}

} // namespace zylann::voxel
