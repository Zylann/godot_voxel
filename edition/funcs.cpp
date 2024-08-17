#include "funcs.h"
#include "../meshers/blocky/voxel_blocky_library_base.h"
#include "../storage/voxel_data.h"
#include "../util/containers/dynamic_bitset.h"
#include "../util/containers/span.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/core/random_pcg.h"
#include "../util/profiling.h"
#include "../util/string/format.h"

#ifdef ZN_GODOT_EXTENSION
using namespace godot;
#endif

namespace zylann::voxel {

void copy_from_chunked_storage(
		VoxelBuffer &dst_buffer,
		Vector3i min_pos,
		unsigned int block_size_po2,
		uint32_t channels_mask,
		const VoxelBuffer *(*get_block_func)(void *, Vector3i),
		void *get_block_func_ctx
) {
	ZN_ASSERT_RETURN_MSG(Vector3iUtil::get_volume(dst_buffer.get_size()) > 0, "The area to copy is empty");
	ZN_ASSERT_RETURN(get_block_func != nullptr);

	const Vector3i max_pos = min_pos + dst_buffer.get_size();

	const Vector3i min_block_pos = min_pos >> block_size_po2;
	const Vector3i max_block_pos = ((max_pos - Vector3i(1, 1, 1)) >> block_size_po2) + Vector3i(1, 1, 1);

	const Vector3i block_size_v = Vector3iUtil::create(1 << block_size_po2);

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				const VoxelBuffer *src_buffer = (*get_block_func)(get_block_func_ctx, bpos);
				const Vector3i src_block_origin = bpos << block_size_po2;

				if (src_buffer != nullptr) {
					for (const uint8_t channel : channels) {
						dst_buffer.set_channel_depth(channel, src_buffer->get_channel_depth(channel));
						// Note: copy_from takes care of clamping the area if it's on an edge
						dst_buffer.copy_channel_from(
								*src_buffer, min_pos - src_block_origin, src_buffer->get_size(), Vector3i(), channel
						);
					}

				} else {
					for (const uint8_t channel : channels) {
						// For now, inexistent blocks default to hardcoded defaults, corresponding to "empty space".
						// If we want to change this, we may have to add an API for that.
						dst_buffer.fill_area(
								VoxelBuffer::get_default_value_static(channel),
								src_block_origin - min_pos,
								src_block_origin - min_pos + block_size_v,
								channel
						);
					}
				}
			}
		}
	}
}

void paste_to_chunked_storage(
		const VoxelBuffer &src_buffer,
		Vector3i min_pos,
		unsigned int block_size_po2,
		unsigned int channels_mask,
		bool use_mask,
		uint8_t mask_channel,
		uint64_t mask_value,
		VoxelBuffer *(*get_block_func)(void *, Vector3i),
		void *get_block_func_ctx
) {
	ZN_ASSERT_RETURN(get_block_func != nullptr);
	const Vector3i max_pos = min_pos + src_buffer.get_size();

	const Vector3i min_block_pos = min_pos >> block_size_po2;
	const Vector3i max_block_pos = ((max_pos - Vector3i(1, 1, 1)) >> block_size_po2) + Vector3i(1, 1, 1);

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				VoxelBuffer *dst_buffer = (*get_block_func)(get_block_func_ctx, bpos);

				if (dst_buffer == nullptr) {
					continue;
				}

				const Vector3i dst_block_origin = bpos << block_size_po2;
				const Vector3i dst_base_pos = min_pos - dst_block_origin;

				if (use_mask) {
					paste_src_masked(
							to_span(channels), src_buffer, mask_channel, mask_value, *dst_buffer, dst_base_pos, true
					);

				} else {
					paste(to_span(channels), src_buffer, *dst_buffer, dst_base_pos, true);
				}
			}
		}
	}
}

AABB get_path_aabb(Span<const Vector3> positions, Span<const float> radii) {
	AABB aabb(positions[0], Vector3());

	for (unsigned int i = 0; i < positions.size(); ++i) {
		const Vector3 pos = positions[i];
		const float r = radii[i];
		const Vector3 extentv(r, r, r);
		aabb = aabb.expand(pos - extentv);
		aabb = aabb.expand(pos + extentv);
	}

	return aabb;
}

void run_blocky_random_tick(
		VoxelData &data,
		Box3i voxel_box,
		const VoxelBlockyLibraryBase &lib,
		RandomPCG &random,
		int voxel_count,
		int batch_count,
		void *callback_data,
		bool (*callback)(void *, Vector3i, int64_t)
) {
	ERR_FAIL_COND(batch_count <= 0);
	ERR_FAIL_COND(voxel_count < 0);
	ERR_FAIL_COND(!math::is_valid_size(voxel_box.size));
	ERR_FAIL_COND(callback == nullptr);

	constexpr unsigned int lod_index = 0;

	const unsigned int block_size = data.get_block_size();
	const Box3i block_box = voxel_box.downscaled(block_size);

	const int block_count = voxel_count / batch_count;
	// const int bs_mask = map.get_block_size_mask();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;

	struct Pick {
		uint64_t value;
		Vector3i rpos;
	};
	static thread_local StdVector<Pick> picks;
	picks.reserve(batch_count);

	const float block_volume = math::cubed(block_size);
	CRASH_COND(block_volume < 0.1f);

	struct L {
		static inline int urand(RandomPCG &random, uint32_t max_value) {
			return random.rand() % max_value;
		}
		static inline Vector3i urand_vec3i(RandomPCG &random, Vector3i s) {
#ifdef DEBUG_ENABLED
			CRASH_COND(s.x <= 0 || s.y <= 0 || s.z <= 0);
#endif
			return Vector3i(urand(random, s.x), urand(random, s.y), urand(random, s.z));
		}
	};

	const VoxelBlockyLibraryBase::BakedData &lib_data = lib.get_baked_data();

	// Choose blocks at random
	for (int bi = 0; bi < block_count; ++bi) {
		const Vector3i block_pos = block_box.position + L::urand_vec3i(random, block_box.size);

		const Vector3i block_origin = data.block_to_voxel(block_pos);

		picks.clear();

		{
			SpatialLock3D &spatial_lock = data.get_spatial_lock(lod_index);
			SpatialLock3D::Read srlock(spatial_lock, BoxBounds3i::from_position(block_pos));

			std::shared_ptr<VoxelBuffer> voxels_ptr = data.try_get_block_voxels(block_pos);

			if (voxels_ptr != nullptr) {
				// Doing ONLY reads here.
				const VoxelBuffer &voxels = *voxels_ptr;

				if (voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
					const uint64_t v = voxels.get_voxel(0, 0, 0, channel);
					if (lib_data.has_model(v)) {
						const VoxelBlockyModel::BakedData &vt = lib_data.models[v];
						if (vt.is_random_tickable) {
							// Skip whole block
							continue;
						}
					}
				}

				const Box3i block_voxel_box(block_origin, Vector3iUtil::create(block_size));
				Box3i local_voxel_box = voxel_box.clipped(block_voxel_box);
				local_voxel_box.position -= block_origin;
				const float volume_ratio = Vector3iUtil::get_volume(local_voxel_box.size) / block_volume;
				const int local_batch_count = Math::ceil(batch_count * volume_ratio);

				// Choose a bunch of voxels at random within the block.
				// Batching this way improves performance a little by reducing block lookups.
				for (int vi = 0; vi < local_batch_count; ++vi) {
					const Vector3i rpos = local_voxel_box.position + L::urand_vec3i(random, local_voxel_box.size);

					const uint64_t v = voxels.get_voxel(rpos, channel);
					picks.push_back(Pick{ v, rpos });
				}
			}
		}

		// The following may or may not read AND write voxels randomly due to its exposition to scripts.
		// However, we don't send the buffer directly, so it will go through an API taking care of locking.
		// So we don't (and shouldn't) lock anything here.
		for (size_t i = 0; i < picks.size(); ++i) {
			const Pick pick = picks[i];

			if (lib_data.has_model(pick.value)) {
				const VoxelBlockyModel::BakedData &vt = lib_data.models[pick.value];

				if (vt.is_random_tickable) {
					ERR_FAIL_COND(!callback(callback_data, pick.rpos + block_origin, pick.value));
				}
			}
		}
	}
}

void run_blocky_random_tick(
		VoxelData &data,
		AABB voxel_box_f,
		const VoxelBlockyLibraryBase &lib,
		RandomPCG &random,
		int voxel_count,
		int batch_count,
		const Callable &callback
) {
	struct CallbackData {
		const Callable &callable;
	};
	CallbackData cb_self{ callback };

	const Box3i voxel_box(math::floor_to_int(voxel_box_f.position), math::floor_to_int(voxel_box_f.size));

	zylann::voxel::run_blocky_random_tick(
			data,
			voxel_box,
			lib,
			random,
			voxel_count,
			batch_count,
			&cb_self,
			[](void *self, Vector3i pos, int64_t val) {
				const CallbackData *cd = reinterpret_cast<const CallbackData *>(self);
#ifdef ZN_GODOT
				const Variant vpos = pos;
				const Variant vv = val;
				const Variant *args[2];
				args[0] = &vpos;
				args[1] = &vv;
				Callable::CallError error;
				Variant retval; // We don't care about the return value, Callable API requires it
				cd->callable.callp(args, 2, retval, error);
				// TODO I would really like to know what's the correct way to report such errors...
				// Examples I found in the engine are inconsistent
				ERR_FAIL_COND_V(error.error != Callable::CallError::CALL_OK, false);
		// Return if it fails, we don't want an error spam
#elif ZN_GODOT_EXTENSION
				// TODO GDX: No way to detect or report errors when calling a Callable. Do I need to?
				cd->callable.call(pos, val);
#endif
				return true;
			}
	);
}

bool indices_to_bitarray_u16(Span<const int32_t> indices, DynamicBitset &bitarray) {
#ifdef DEBUG_ENABLED
	const int32_t max_supported_value = 65535;
	// Validate
	for (const int32_t i : indices) {
		ZN_ASSERT_RETURN_V_MSG(
				i >= 0 && i <= max_supported_value,
				false,
				format("Index {} is out of supported range 0..{}", i, max_supported_value)
		);
		return false;
	}
#endif

	int32_t max_value = 0;
	for (const int32_t i : indices) {
		max_value = math::max(i, max_value);
	}

	bitarray.resize_no_init(indices.size() / 8);

	for (const int32_t i : indices) {
		bitarray.set(i);
	}

	return true;
}

} // namespace zylann::voxel

namespace zylann::voxel::ops {

Box3i get_round_cone_int_bounds(Vector3 p0, Vector3 p1, float r0, float r1) {
	const Vector3 minp( //
			math::min(p0.x - r0, p1.x - r1), //
			math::min(p0.y - r0, p1.y - r1), //
			math::min(p0.z - r0, p1.z - r1)
	);

	const Vector3 maxp( //
			math::max(p0.x + r0, p1.x + r1), //
			math::max(p0.y + r0, p1.y + r1), //
			math::max(p0.z + r0, p1.z + r1)
	);

	return Box3i::from_min_max(to_vec3i(math::floor(minp)), to_vec3i(math::ceil(maxp)));
}

#ifdef DEBUG_ENABLED

// Reference implementation. Correct but very slow.
void box_blur_slow_ref(const VoxelBuffer &src, VoxelBuffer &dst, int radius, Vector3f sphere_pos, float sphere_radius) {
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
						src.get_voxel_f(dst_pos + Vector3i(radius, radius, radius), VoxelBuffer::CHANNEL_SDF);

				const float sphere_ds = math::distance_squared(sphere_pos, to_vec3f(dst_pos));
				if (sphere_ds > sphere_radius_s) {
					// Outside of brush
					dst.set_voxel_f(sd_src, dst_pos, VoxelBuffer::CHANNEL_SDF);
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
							sd_sum += src.get_voxel_f(src_pos, VoxelBuffer::CHANNEL_SDF);
						}
					}
				}

				const float sd_avg = sd_sum / box_volume;
				const float sd = Math::lerp(sd_src, sd_avg, factor);

				dst.set_voxel_f(sd, dst_pos, VoxelBuffer::CHANNEL_SDF);
			}
		}
	}
}

#endif

void box_blur(const VoxelBuffer &src, VoxelBuffer &dst, int radius, Vector3f sphere_pos, float sphere_radius) {
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
	StdVector<float> ring_buffer;
	const unsigned int rb_power = math::get_next_power_of_two_32_shift(box_size);
	const unsigned int rb_len = 1 << rb_power;
	ring_buffer.resize(rb_len);
	const unsigned int rb_mask = rb_len - 1;
	ZN_ASSERT(static_cast<int>(ring_buffer.size()) >= box_size);

	// Temporary buffer with extra length in two axes
	StdVector<float> tmp;
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
					const float sd = src.get_voxel_f(Vector3i(dst_pos.x, y, dst_pos.z), VoxelBuffer::CHANNEL_SDF);
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
							Vector3i(dst_pos.x, dst_pos.y + radius * 2, dst_pos.z), VoxelBuffer::CHANNEL_SDF
					);
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
					const float src_sd = src.get_voxel_f(src_pos, VoxelBuffer::CHANNEL_SDF);

					const float sphere_ds = math::distance_squared(sphere_pos, to_vec3f(dst_pos));
					if (sphere_ds > sphere_radius_s) {
						// Outside of brush
						dst.set_voxel_f(src_sd, dst_pos, VoxelBuffer::CHANNEL_SDF);
						continue;
					}

					// Brush factor
					const float factor = math::clamp(1.f - sphere_ds / sphere_radius_s, 0.f, 1.f);

					const Vector3i tmp_pos(dst_pos.x + radius, dst_pos.y, dst_pos.z + radius);
					const unsigned int tmp_loc = Vector3iUtil::get_zxy_index(tmp_pos, tmp_size);
					const float tmp_sd = tmp[tmp_loc];

					const float sd = Math::lerp(src_sd, tmp_sd, factor);
					dst.set_voxel_f(sd, dst_pos, VoxelBuffer::CHANNEL_SDF);
				}
			}
		}
	}
}

void grow_sphere(VoxelBuffer &src, float strength, Vector3f sphere_pos, float sphere_radius) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(sphere_radius > 0.001f);

	const Vector3i src_size = src.get_size();

	ZN_ASSERT_RETURN(src_size.x >= 0);
	ZN_ASSERT_RETURN(src_size.y >= 0);
	ZN_ASSERT_RETURN(src_size.z >= 0);

	const float sphere_radius_squared = sphere_radius * sphere_radius;
	const float inv_sphere_radius = 1.f / sphere_radius;

	Vector3i src_pos;
	for (src_pos.z = 0; src_pos.z < src_size.z; ++src_pos.z) {
		for (src_pos.x = 0; src_pos.x < src_size.x; ++src_pos.x) {
			for (src_pos.y = 0; src_pos.y < src_size.y; ++src_pos.y) {
				const float src_sd = src.get_voxel_f(src_pos, VoxelBuffer::CHANNEL_SDF);

				const float sphere_ds = math::distance_squared(sphere_pos, to_vec3f(src_pos));
				if (sphere_ds > sphere_radius_squared) {
					// Outside of brush
					continue;
				}

				const float distance = Math::sqrt(sphere_ds);
				const float sd_offset = strength * (sphere_radius - distance) * inv_sphere_radius;

				// With signed distance fields, subtracting "grows" the shape.
				// Negative strength is allowed so it can also be used to "shrink".
				src.set_voxel_f(src_sd - sd_offset, src_pos, VoxelBuffer::CHANNEL_SDF);
			}
		}
	}
}

} // namespace zylann::voxel::ops
