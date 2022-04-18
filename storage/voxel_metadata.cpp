#include "voxel_metadata.h"
#include "../util/errors.h"
#include "../util/string_funcs.h"

namespace zylann::voxel {

void VoxelMetadata::clear() {
	if (_type >= TYPE_CUSTOM_BEGIN) {
		ZN_DELETE(_data.custom_data);
		_data.custom_data = nullptr;
	}
	_type = TYPE_EMPTY;
}

void VoxelMetadata::set_u64(const uint64_t &v) {
	if (_type != TYPE_U64) {
		clear();
		_type = TYPE_U64;
	}
	_data.u64_data = v;
}

uint64_t VoxelMetadata::get_u64() const {
	ZN_ASSERT(_type == TYPE_U64);
	return _data.u64_data;
}

void VoxelMetadata::set_custom(uint8_t type, ICustomVoxelMetadata *custom_data) {
	ZN_ASSERT(type >= TYPE_CUSTOM_BEGIN);
	clear();
	_type = type;
	_data.custom_data = custom_data;
}

ICustomVoxelMetadata &VoxelMetadata::get_custom() {
	ZN_ASSERT(_type >= TYPE_CUSTOM_BEGIN);
	ZN_ASSERT(_data.custom_data != nullptr);
	return *_data.custom_data;
}

const ICustomVoxelMetadata &VoxelMetadata::get_custom() const {
	ZN_ASSERT(_type >= TYPE_CUSTOM_BEGIN);
	ZN_ASSERT(_data.custom_data != nullptr);
	return *_data.custom_data;
}

void VoxelMetadata::copy_from(const VoxelMetadata &src) {
	clear();
	if (src._type >= TYPE_CUSTOM_BEGIN) {
		ZN_ASSERT(src._data.custom_data != nullptr);
		_data.custom_data = src._data.custom_data->duplicate();
	} else {
		_data = src._data;
	}
	_type = src._type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
