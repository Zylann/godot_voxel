#ifndef VOXEL_METADATA_FACTORY_H
#define VOXEL_METADATA_FACTORY_H

#include "../../util/containers/fixed_array.h"
#include "voxel_metadata.h"
#include <cstdint>

namespace zylann::voxel {

// Registry of custom metadata types, used to deserialize them from saved data.
class VoxelMetadataFactory {
public:
	typedef ICustomVoxelMetadata *(*ConstructorFunc)();

	static VoxelMetadataFactory &get_singleton();

	VoxelMetadataFactory();

	// Registers a custom metadata type.
	// The `type` you choose should remain the same over time.
	// It will be used in save files, so changing it could break old saves.
	// `type` must be greater or equal to `VoxelMetadata::TYPE_CUSTOM_BEGIN`.
	void add_constructor(uint8_t type, ConstructorFunc ctor);

	template <typename T>
	void add_constructor_by_type(uint8_t type) {
		add_constructor(type, []() { //
			// Doesn't compile if I directly return the newed instance
			ICustomVoxelMetadata *c = ZN_NEW(T);
			return c;
		});
	}

	void remove_constructor(uint8_t type);

	// Constructs a custom metadata type from the given type ID.
	// The `type` must be greater or equal to `VoxelMetadata::TYPE_CUSTOM_BEGIN`.
	// Returns `nullptr` if the type could not be constructed.
	ICustomVoxelMetadata *try_construct(uint8_t type) const;

private:
	FixedArray<ConstructorFunc, VoxelMetadata::CUSTOM_TYPES_MAX_COUNT> _constructors;
};

} // namespace zylann::voxel

#endif // VOXEL_METADATA_FACTORY_H
