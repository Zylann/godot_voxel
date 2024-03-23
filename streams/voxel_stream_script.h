#ifndef VOXEL_STREAM_SCRIPT_H
#define VOXEL_STREAM_SCRIPT_H

#include "../util/godot/core/gdvirtual.h"
#include "voxel_stream.h"

#ifdef ZN_GODOT_EXTENSION
// GodotCpp wants the full definition of the class in GDVIRTUAL
#include "../storage/voxel_buffer_gd.h"
#endif

namespace zylann::voxel {

// Provides access to a source of paged voxel data, which may load and save.
// Must be implemented in a multi-thread-safe way.
// If you are looking for a more specialized API to generate voxels, use VoxelGenerator.
class VoxelStreamScript : public VoxelStream {
	GDCLASS(VoxelStreamScript, VoxelStream)
public:
	void load_voxel_block(VoxelStream::VoxelQueryData &q) override;
	void save_voxel_block(VoxelStream::VoxelQueryData &q) override;

	int get_used_channels_mask() const override;

protected:
	// TODO Why is it unable to convert `Result` into `Variant` even though a cast is defined in voxel_stream.h???
	GDVIRTUAL3R(int, _load_voxel_block, Ref<godot::VoxelBuffer>, Vector3i, int)
	GDVIRTUAL3(_save_voxel_block, Ref<godot::VoxelBuffer>, Vector3i, int)
	GDVIRTUAL0RC(int, _get_used_channels_mask) // I think `C` means `const`?

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_STREAM_SCRIPT_H
