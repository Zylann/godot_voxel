#include "funcs.h"
#include "../util/profiling.h"

namespace zylann::voxel::ops {

// Reference implementation. Correct but very slow.
void box_blur_slow_ref(const VoxelBufferInternal &src, VoxelBufferInternal &dst, int radius, Vector3f sphere_pos,
		float sphere_radius) {
	ZN_PROFILE_SCOPE();

	const Vector3i dst_size = src.get_size() - Vector3i(radius, radius, radius) * 2;

	ZN_ASSERT_RETURN(dst_size.x >= 0);
	ZN_ASSERT_RETURN(dst_size.y >= 0);
	ZN_ASSERT_RETURN(dst_size.z >= 0);

	dst.create(dst_size);

	const int box_size = radius * 2 + 1;
	const float box_volume = Vector3iUtil::get_volume(Vector3i(box_size, box_size, box_size));

	const float sphere_radius_s = sphere_radius * sphere_radius;

	Vector3i dst_pos;
	for (dst_pos.z = 0; dst_pos.z < dst.get_size().z; ++dst_pos.z) {
		for (dst_pos.x = 0; dst_pos.x < dst.get_size().x; ++dst_pos.x) {
			for (dst_pos.y = 0; dst_pos.y < dst.get_size().y; ++dst_pos.y) {
				const float sd_src =
						src.get_voxel_f(dst_pos + Vector3i(radius, radius, radius), VoxelBufferInternal::CHANNEL_SDF);

				const float sphere_ds = math::distance_squared(sphere_pos, to_vec3f(dst_pos));
				if (sphere_ds > sphere_radius_s) {
					// Outside of brush
					dst.set_voxel_f(sd_src, dst_pos, VoxelBufferInternal::CHANNEL_SDF);
					continue;
				}
				// Brush factor
				const float factor = math::clamp(1.f - sphere_ds / sphere_radius_s, 0.f, 1.f);

				const Vector3i src_min = dst_pos; // - Vector3i(radius, radius, radius);
				const Vector3i src_max = src_min + Vector3i(box_size, box_size, box_size);

				Vector3i src_pos;
				float sd_sum = 0.f;
				for (src_pos.z = src_min.z; src_pos.z < src_max.z; ++src_pos.z) {
					for (src_pos.x = src_min.x; src_pos.x < src_max.x; ++src_pos.x) {
						for (src_pos.y = src_min.y; src_pos.y < src_max.y; ++src_pos.y) {
							// This is a hotspot. Could be optimized by separating XYZ blurs and caching reads in a
							// ringbuffer
							sd_sum += src.get_voxel_f(src_pos, VoxelBufferInternal::CHANNEL_SDF);
						}
					}
				}

				const float sd_avg = sd_sum / box_volume;
				const float sd = Math::lerp(sd_src, sd_avg, factor);

				dst.set_voxel_f(sd, dst_pos, VoxelBufferInternal::CHANNEL_SDF);
			}
		}
	}
}

void box_blur(const VoxelBufferInternal &src, VoxelBufferInternal &dst, int radius, Vector3f sphere_pos,
		float sphere_radius) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(radius >= 1);

	const Vector3i dst_size = src.get_size() - Vector3i(radius, radius, radius) * 2;

	ZN_ASSERT_RETURN(dst_size.x >= 0);
	ZN_ASSERT_RETURN(dst_size.y >= 0);
	ZN_ASSERT_RETURN(dst_size.z >= 0);

	dst.create(dst_size);

	const int box_size = radius * 2 + 1;
	const float box_size_f = box_size;
	// const float box_volume = Vector3iUtil::get_volume(Vector3i(box_size, box_size, box_size));

	const float sphere_radius_s = sphere_radius * sphere_radius;

	// Box blur is separable: we can do 1-dimensional blur along the first axis, then the second axis, then the third
	// axis. This reduces the amount of memory accesses, and simplifies the algorithm to 1-D.

	// Since separated blur is 1-D, we can use a ring buffer to optimize reading/summing values since we are
	// just moving an averaging window by 1 voxel on each iteration. So no need to gather all values to average them on
	// each iteration, we just get one and remove one.
	std::vector<float> ring_buffer;
	const unsigned int rb_power = math::get_next_power_of_two_32_shift(box_size);
	const unsigned int rb_len = 1 << rb_power;
	ring_buffer.resize(rb_len);
	const unsigned int rb_mask = rb_len - 1;
	ZN_ASSERT(static_cast<int>(ring_buffer.size()) >= box_size);

	// Temporary buffer with extra length in two axes
	std::vector<float> tmp;
	const Vector3i tmp_size(dst_size.x + 2 * radius, dst_size.y, dst_size.z + 2 * radius);
	tmp.resize(Vector3iUtil::get_volume(tmp_size));

	// Y blur
	Vector3i dst_pos;
	unsigned int tmp_stride = 1;
	unsigned int tmp_i = 0;
	{
		ZN_PROFILE_SCOPE_NAMED("Y blur");
		for (dst_pos.z = 0; dst_pos.z < tmp_size.z; ++dst_pos.z) {
			for (dst_pos.x = 0; dst_pos.x < tmp_size.x; ++dst_pos.x) {
				float sd_sum = 0.f;
				// Fill window with initial samples
				for (int y = 0; y < box_size; ++y) {
					// TODO The fact we sample this way for the first axis makes it a lot slower than the others.
					// Make tmp larger to fit the whole size and convert first into it?
					const float sd =
							src.get_voxel_f(Vector3i(dst_pos.x, y, dst_pos.z), VoxelBufferInternal::CHANNEL_SDF);
					ring_buffer[y] = sd;
					sd_sum += sd;
				}

				tmp[tmp_i] = sd_sum / box_size_f;
				tmp_i += tmp_stride;

				// Read/write cursors in ring buffer:
				// Assuming we move the window "from left to right"
				int rbr = 0; // Left-most sample in the window
				int rbw = box_size & rb_mask; // Right-most sample in the window + 1

				for (dst_pos.y = 1; dst_pos.y < tmp_size.y; ++dst_pos.y) {
					// Look 2*radius ahead because we sample from a buffer that's also bigger than tmp in Y
					const float sd = src.get_voxel_f(
							Vector3i(dst_pos.x, dst_pos.y + radius * 2, dst_pos.z), VoxelBufferInternal::CHANNEL_SDF);
					// Remove sample exiting the window
					sd_sum -= ring_buffer[rbr];
					// Add sample entering the window
					sd_sum += sd;
					ring_buffer[rbw] = sd;
					// Advance read and write cursors by 1. Masking handles wrapping around the ring buffer (faster than
					// modulus)
					rbr = (rbr + 1) & rb_mask;
					rbw = (rbw + 1) & rb_mask;

					tmp[tmp_i] = sd_sum / box_size_f;
					tmp_i += tmp_stride;
				}
			}
		}
	}

	// X blur
	{
		ZN_PROFILE_SCOPE_NAMED("X blur");
		tmp_stride = tmp_size.y;
		for (dst_pos.z = 0; dst_pos.z < tmp_size.z; ++dst_pos.z) {
			for (dst_pos.y = 0; dst_pos.y < tmp_size.y; ++dst_pos.y) {
				// Initialize on each row, because we don't do a fully regular access this time
				tmp_i = Vector3iUtil::get_zxy_index(Vector3i(0, dst_pos.y, dst_pos.z), tmp_size);

				float sd_sum = 0.f;
				for (int x = 0; x < box_size; ++x) {
					// This time we read samples from our temporary buffer itself.
					// We are writing while we read the buffer, but it should be ok since we only read values
					// before they get modified, and it won't retroactively affect the result when we reach them because
					// the ringbuffer serves as copy of those values.
					const float sd = tmp[tmp_i + x * tmp_stride];
					ring_buffer[x] = sd;
					sd_sum += sd;
				}

				// Compute X blur only for the final area (we had neighbors initially to capture blurred samples along
				// Y, which X and Z will use)

				tmp_i += radius * tmp_stride; // Jump by +(radius, 0, 0)
				tmp[tmp_i] = sd_sum / box_size_f;

				int rbr = 0;
				int rbw = box_size & rb_mask;

				for (dst_pos.x = radius + 1; dst_pos.x < tmp_size.x - radius; ++dst_pos.x) {
					tmp_i += tmp_stride;

					const float sd = tmp[tmp_i + radius * tmp_stride];
					sd_sum -= ring_buffer[rbr];
					sd_sum += sd;
					ring_buffer[rbw] = sd;
					// Advance read and write cursors by 1
					rbr = (rbr + 1) & rb_mask;
					rbw = (rbw + 1) & rb_mask;

					tmp[tmp_i] = sd_sum / box_size_f;
				}
			}
		}
	}

	// Z blur
	{
		ZN_PROFILE_SCOPE_NAMED("Z blur");
		tmp_stride = tmp_size.y * tmp_size.x;
		for (dst_pos.x = radius; dst_pos.x < tmp_size.x - radius; ++dst_pos.x) {
			for (dst_pos.y = 0; dst_pos.y < tmp_size.y; ++dst_pos.y) {
				tmp_i = Vector3iUtil::get_zxy_index(Vector3i(dst_pos.x, dst_pos.y, 0), tmp_size);

				float sd_sum = 0.f;
				for (int z = 0; z < box_size; ++z) {
					const float sd = tmp[tmp_i + z * tmp_stride];
					ring_buffer[z] = sd;
					sd_sum += sd;
				}

				tmp_i += radius * tmp_stride; // Jump by +(0, 0, radius)
				tmp[tmp_i] = sd_sum / box_size_f;

				int rbr = 0;
				int rbw = box_size & rb_mask;

				for (dst_pos.z = radius + 1; dst_pos.z < tmp_size.z - radius; ++dst_pos.z) {
					tmp_i += tmp_stride;

					const float sd = tmp[tmp_i + radius * tmp_stride];
					sd_sum -= ring_buffer[rbr];
					sd_sum += sd;
					ring_buffer[rbw] = sd;
					// Advance read and write cursors by 1
					rbr = (rbr + 1) & rb_mask;
					rbw = (rbw + 1) & rb_mask;

					tmp[tmp_i] = sd_sum / box_size_f;
				}
			}
		}
	}

	// Blend using shape

	{
		ZN_PROFILE_SCOPE_NAMED("Blend");
		for (dst_pos.z = 0; dst_pos.z < dst_size.z; ++dst_pos.z) {
			for (dst_pos.x = 0; dst_pos.x < dst_size.x; ++dst_pos.x) {
				for (dst_pos.y = 0; dst_pos.y < dst_size.y; ++dst_pos.y) {
					//
					const Vector3i src_pos = dst_pos + Vector3i(radius, radius, radius);
					// TODO It might be possible to optimize this read
					const float src_sd = src.get_voxel_f(src_pos, VoxelBufferInternal::CHANNEL_SDF);

					const float sphere_ds = math::distance_squared(sphere_pos, to_vec3f(dst_pos));
					if (sphere_ds > sphere_radius_s) {
						// Outside of brush
						dst.set_voxel_f(src_sd, dst_pos, VoxelBufferInternal::CHANNEL_SDF);
						continue;
					}

					// Brush factor
					const float factor = math::clamp(1.f - sphere_ds / sphere_radius_s, 0.f, 1.f);

					const Vector3i tmp_pos(dst_pos.x + radius, dst_pos.y, dst_pos.z + radius);
					const unsigned int tmp_loc = Vector3iUtil::get_zxy_index(tmp_pos, tmp_size);
					const float tmp_sd = tmp[tmp_loc];

					const float sd = Math::lerp(src_sd, tmp_sd, factor);
					dst.set_voxel_f(sd, dst_pos, VoxelBufferInternal::CHANNEL_SDF);
				}
			}
		}
	}
}

} // namespace zylann::voxel::ops
