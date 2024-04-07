#ifndef VOXEL_CUSTOM_METADATA_H
#define VOXEL_CUSTOM_METADATA_H

#include "../../util/containers/span.h"

namespace zylann::voxel {

// Base interface for custom data types.
class ICustomVoxelMetadata {
public:
	virtual ~ICustomVoxelMetadata() {}

	// Gets how many bytes this metadata will occupy when serialized.
	virtual size_t get_serialized_size() const = 0;

	// Serializes this metadata into `dst`. The size of `dst` will be equal or greater than the size returned by
	// `get_serialized_size()`. Returns how many bytes were written.
	virtual size_t serialize(Span<uint8_t> dst) const = 0;

	// Deserializes this metadata from the given bytes.
	// Returns `true` on success, `false` otherwise. `out_read_size` must be assigned to the number of bytes read.
	virtual bool deserialize(Span<const uint8_t> src, uint64_t &out_read_size) = 0;

	virtual ICustomVoxelMetadata *duplicate() = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_CUSTOM_METADATA_H
