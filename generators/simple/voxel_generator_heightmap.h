#ifndef VOXEL_GENERATOR_HEIGHTMAP_H
#define VOXEL_GENERATOR_HEIGHTMAP_H

#include "../../storage/voxel_buffer.h"
#include "../voxel_generator.h"
#include <core/image.h>

class VoxelGeneratorHeightmap : public VoxelGenerator {
	GDCLASS(VoxelGeneratorHeightmap, VoxelGenerator)
public:
	VoxelGeneratorHeightmap();
	~VoxelGeneratorHeightmap();

	void set_channel(VoxelBuffer::ChannelId p_channel);
	VoxelBuffer::ChannelId get_channel() const;
	int get_used_channels_mask() const override;

	void set_height_start(float start);
	float get_height_start() const;

	void set_height_range(float range);
	float get_height_range() const;

	void set_iso_scale(float iso_scale);
	float get_iso_scale() const;

protected:
	template <typename Height_F>
	Result generate(VoxelBufferInternal &out_buffer, Height_F height_func, Vector3i origin, int lod) {
		Parameters params;
		{
			RWLockRead rlock(_parameters_lock);
			params = _parameters;
		}

		const int channel = params.channel;
		const Vector3i bs = out_buffer.get_size();
		const bool use_sdf = channel == VoxelBufferInternal::CHANNEL_SDF;

		if (origin.y > get_height_start() + get_height_range()) {
			// The bottom of the block is above the highest ground can go (default is air)
			Result result;
			result.max_lod_hint = true;
			return result;
		}
		if (origin.y + (bs.y << lod) < get_height_start()) {
			// The top of the block is below the lowest ground can go
			out_buffer.clear_channel(params.channel, use_sdf ? 0 : params.matter_type);
			Result result;
			result.max_lod_hint = true;
			return result;
		}

		const int stride = 1 << lod;

		if (use_sdf) {
			int gz = origin.z;

			for (int z = 0; z < bs.z; ++z, gz += stride) {
				int gx = origin.x;

				for (int x = 0; x < bs.x; ++x, gx += stride) {
					float h = params.range.xform(height_func(gx, gz));
					int gy = origin.y;
					for (int y = 0; y < bs.y; ++y, gy += stride) {
						float sdf = params.iso_scale * (gy - h);
						out_buffer.set_voxel_f(sdf, x, y, z, channel);
					}

				} // for x
			} // for z

		} else {
			// Blocky

			int gz = origin.z;

			for (int z = 0; z < bs.z; ++z, gz += stride) {
				int gx = origin.x;

				for (int x = 0; x < bs.x; ++x, gx += stride) {
					// Output is blocky, so we can go for just one sample
					float h = params.range.xform(height_func(gx, gz));
					h -= origin.y;
					int ih = int(h) >> lod;
					if (ih > 0) {
						if (ih > bs.y) {
							ih = bs.y;
						}
						out_buffer.fill_area(
								params.matter_type, Vector3i(x, 0, z), Vector3i(x + 1, ih, z + 1), channel);
					}

				} // for x
			} // for z
		} // use_sdf

		return Result();
	}

private:
	static void _bind_methods();

	struct Range {
		float start = -50.f;
		float height = 200.f;

		inline float xform(float x) const {
			return x * height + start;
		}
	};

	struct Parameters {
		VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
		int matter_type = 1;
		Range range;
		float iso_scale = 0.1;
	};

	RWLock _parameters_lock;
	Parameters _parameters;
};

#endif // VOXEL_GENERATOR_HEIGHTMAP_H
