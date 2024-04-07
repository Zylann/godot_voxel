#include "voxel_metadata_factory.h"
#include "../../util/errors.h"
#include "../../util/string/format.h"

namespace zylann::voxel {

namespace {
VoxelMetadataFactory g_voxel_metadata_factory;
}

VoxelMetadataFactory &VoxelMetadataFactory::get_singleton() {
	return g_voxel_metadata_factory;
}

VoxelMetadataFactory::VoxelMetadataFactory() {
	fill(_constructors, (ConstructorFunc) nullptr);
}

void VoxelMetadataFactory::add_constructor(uint8_t type, ConstructorFunc ctor) {
	ZN_ASSERT(type >= VoxelMetadata::TYPE_CUSTOM_BEGIN);
	const unsigned int i = type - VoxelMetadata::TYPE_CUSTOM_BEGIN;
	ZN_ASSERT_MSG(_constructors[i] == nullptr, "Type already registered");
	_constructors[i] = ctor;
}

void VoxelMetadataFactory::remove_constructor(uint8_t type) {
	ZN_ASSERT(type >= VoxelMetadata::TYPE_CUSTOM_BEGIN);
	const unsigned int i = type - VoxelMetadata::TYPE_CUSTOM_BEGIN;
	ZN_ASSERT_MSG(_constructors[i] != nullptr, "Type not registered");
	_constructors[i] = nullptr;
}

ICustomVoxelMetadata *VoxelMetadataFactory::try_construct(uint8_t type) const {
	ZN_ASSERT_RETURN_V_MSG(
			type >= VoxelMetadata::TYPE_CUSTOM_BEGIN, nullptr, format("Invalid custom metadata type {}", type));
	const unsigned int i = type - VoxelMetadata::TYPE_CUSTOM_BEGIN;

	const ConstructorFunc ctor = _constructors[i];
	ZN_ASSERT_RETURN_V_MSG(ctor != nullptr, nullptr, format("Custom metadata constructor not found for type {}", type));

	ICustomVoxelMetadata *m = ctor();
	ZN_ASSERT_RETURN_V_MSG(
			m != nullptr, nullptr, format("Custom metadata constructor for type {} returned nullptr", type));

	return ctor();
}

} // namespace zylann::voxel
