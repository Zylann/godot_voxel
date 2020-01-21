#ifndef VOXEL_STREAM_HEIGHTMAP_H
#define VOXEL_STREAM_HEIGHTMAP_H

#include "../util/heightmap_sdf.h"
#include "voxel_stream.h"
#include <core/image.h>

class VoxelStreamHeightmap : public VoxelStream {
	GDCLASS(VoxelStreamHeightmap, VoxelStream)
public:
	VoxelStreamHeightmap();

	enum SdfMode {
		SDF_VERTICAL = HeightmapSdf::SDF_VERTICAL,
		SDF_VERTICAL_AVERAGE = HeightmapSdf::SDF_VERTICAL_AVERAGE,
		SDF_SEGMENT = HeightmapSdf::SDF_SEGMENT,
		SDF_MODE_COUNT = HeightmapSdf::SDF_MODE_COUNT
	};

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;

	void set_sdf_mode(SdfMode mode);
	SdfMode get_sdf_mode() const;

	void set_height_base(float base);
	float get_height_base() const;

	void set_height_range(float range);
	float get_height_range() const;

protected:
	template <typename Height_F>
	void generate(VoxelBuffer &out_buffer, Height_F height_func, int ox, int oy, int oz, int lod) {

		const int channel = _channel;
		const Vector3i bs = out_buffer.get_size();
		bool use_sdf = channel == VoxelBuffer::CHANNEL_SDF;

		if (oy > get_height_base() + get_height_range()) {
			// The bottom of the block is above the highest ground can go (default is air)
			return;
		}
		if (oy + (bs.y << lod) < get_height_base()) {
			// The top of the block is below the lowest ground can go
			out_buffer.clear_channel(_channel, use_sdf ? 0 : _matter_type);
			return;
		}

		const int stride = 1 << lod;

		if (use_sdf) {

			if (lod == 0) {
				// When sampling SDF, we may need to precompute values to speed it up depending on the chosen mode.
				// Unfortunately, only LOD0 can use a cache. lower lods would require a much larger one,
				// otherwise it would interpolate along higher stride, thus voxel values depend on LOD, and then cause discontinuities.
				_heightmap.build_cache(height_func, bs.x, bs.z, ox, oz, stride);
			}

			for (int z = 0; z < bs.z; ++z) {
				for (int x = 0; x < bs.x; ++x) {

					// SDF may vary along the column so we use a helper for more precision

					if (lod == 0) {
						_heightmap.get_column_from_cache(
								[&out_buffer, x, z, channel](int ly, float v) { out_buffer.set_voxel_f(v, x, ly, z, channel); },
								x, oy, z, bs.y, stride);
					} else {
						HeightmapSdf::get_column_stateless(
								[&out_buffer, x, z, channel](int ly, float v) { out_buffer.set_voxel_f(v, x, ly, z, channel); },
								[&height_func, this](int lx, int lz) { return _heightmap.settings.range.xform(height_func(lx, lz)); },
								_heightmap.settings.mode,
								ox + (x << lod), oy, oz + (z << lod), stride, bs.y);
					}

				} // for x
			} // for z

		} else {
			// Blocky

			int gz = oz;
			for (int z = 0; z < bs.z; ++z, gz += stride) {

				int gx = ox;
				for (int x = 0; x < bs.x; ++x, gx += stride) {

					// Output is blocky, so we can go for just one sample
					float h = _heightmap.settings.range.xform(height_func(gx, gz));
					h -= oy;
					int ih = int(h);
					if (ih > 0) {
						if (ih > bs.y) {
							ih = bs.y;
						}
						out_buffer.fill_area(_matter_type, Vector3i(x, 0, z), Vector3i(x + 1, ih, z + 1), channel);
					}

				} // for x
			} // for z
		} // use_sdf
	}

private:
	static void _bind_methods();

private:
	HeightmapSdf _heightmap;
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_TYPE;
	int _matter_type = 1;
};

VARIANT_ENUM_CAST(VoxelStreamHeightmap::SdfMode)

#endif // VOXEL_STREAM_HEIGHTMAP_H
