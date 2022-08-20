#include "object.h"
#include "../profiling.h"

namespace zylann {

uint64_t get_deep_hash(const Object &obj, uint32_t property_usage, uint64_t hash) {
	ZN_PROFILE_SCOPE();

	hash = hash_djb2_one_64(obj.get_class_name().hash(), hash);

	List<PropertyInfo> properties;
	obj.get_property_list(&properties, false);

	// I'd like to use ConstIterator since I only read that list but that isn't possible :shrug:
	for (List<PropertyInfo>::Iterator it = properties.begin(); it != properties.end(); ++it) {
		const PropertyInfo property = *it;

		if ((property.usage & property_usage) != 0) {
			const Variant value = obj.get(property.name);
			uint64_t value_hash = 0;

			if (value.get_type() == Variant::OBJECT) {
				const Object *obj_value = value.operator Object *();
				if (obj_value != nullptr) {
					value_hash = get_deep_hash(*obj_value, property_usage, hash);
				}

			} else {
				value_hash = value.hash();
			}

			hash = hash_djb2_one_64(value_hash, hash);
		}
	}

	return hash;
}

} //namespace zylann
