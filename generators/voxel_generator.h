#ifndef VOXEL_GENERATOR_H
#define VOXEL_GENERATOR_H

#include "../streams/voxel_stream.h"

// TODO I would like VoxelGenerator to not inherit VoxelStream
// because it gets members that make no sense with generators

// Provides access to read-only generated voxels.
// Must be implemented in a multi-thread-safe way.
class VoxelGenerator : public VoxelStream {
	GDCLASS(VoxelGenerator, VoxelStream)
public:
	VoxelGenerator();

	virtual void generate_block(VoxelBlockRequest &input);
	// TODO Single sample

	//	virtual bool is_thread_safe() const;
	//	virtual bool is_cloneable() const;

private:
	void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) override;

protected:
	static void _bind_methods();

	void _b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
};

#endif // VOXEL_GENERATOR_H
