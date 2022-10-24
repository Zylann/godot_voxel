#ifndef VOXEL_STREAM_SCRIPT_H
#define VOXEL_STREAM_SCRIPT_H

#include "voxel_stream.h"
#if defined(ZN_GODOT)
#include <core/object/script_language.h> // needed for GDVIRTUAL macro
#include <core/object/gdvirtual.gen.inc> // Also needed for GDVIRTUAL macro...
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
// TODO GDX: Defining custom virtual functions is not supported...
#if defined(ZN_GODOT)
	// TODO Why is it unable to convert `Result` into `Variant` even though a cast is defined in voxel_stream.h???
	//GDVIRTUAL3R(VoxelStream::Result, _emerge_block, Ref<VoxelBuffer>, Vector3i, int)
	GDVIRTUAL3R(int, _load_voxel_block, Ref<gd::VoxelBuffer>, Vector3i, int)
	GDVIRTUAL3(_save_voxel_block, Ref<gd::VoxelBuffer>, Vector3i, int)
	GDVIRTUAL0RC(int, _get_used_channels_mask) // I think `C` means `const`?
#endif

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_STREAM_SCRIPT_H
