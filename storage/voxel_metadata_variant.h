#ifndef VOXEL_METADATA_VARIANT_H
#define VOXEL_METADATA_VARIANT_H

#include "../util/godot/core/variant.h"
#include "voxel_metadata.h"

namespace zylann::voxel::gd {

// TODO Not sure if that should be a custom type. Custom types are supposed to be specific to a game?
enum GodotMetadataTypes { //
	METADATA_TYPE_VARIANT = VoxelMetadata::TYPE_CUSTOM_BEGIN
};

// Custom metadata holding a Godot Variant (basically, anything recognized by Godot Engine).
// Serializability depends on the same rules as Godot's `encode_variant`: no invalid objects, no cycles.
class VoxelMetadataVariant : public ICustomVoxelMetadata {
public:
	Variant data;

	size_t get_serialized_size() const override;
	size_t serialize(Span<uint8_t> dst) const override;
	bool deserialize(Span<const uint8_t> src, uint64_t &out_read_size) override;
	ICustomVoxelMetadata *duplicate() override;
};

Variant get_as_variant(const VoxelMetadata &meta);
void set_as_variant(VoxelMetadata &meta, Variant v);

} // namespace zylann::voxel::gd

#endif // VOXEL_METADATA_VARIANT_H
