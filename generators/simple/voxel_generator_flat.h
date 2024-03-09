#ifndef VOXEL_GENERATOR_FLAT_H
#define VOXEL_GENERATOR_FLAT_H

#include "../../constants/voxel_constants.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_generator.h"

namespace zylann::voxel {

class VoxelGeneratorFlat : public VoxelGenerator {
	GDCLASS(VoxelGeneratorFlat, VoxelGenerator)

public:
	VoxelGeneratorFlat();
	~VoxelGeneratorFlat();

	void set_channel(VoxelBuffer::ChannelId p_channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;

	void set_voxel_type(int t);
	int get_voxel_type() const;

	void set_height(float h);
	float get_height() const;

protected:
	static void _bind_methods();

private:
	void _b_set_channel(godot::VoxelBuffer::ChannelId p_channel);
	godot::VoxelBuffer::ChannelId _b_get_channel() const;

	struct Parameters {
		VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
		int voxel_type = 1;
		float height = 0;
		float iso_scale = constants::QUANTIZED_SDF_16_BITS_SCALE;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_FLAT_H
