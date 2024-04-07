#ifndef VOXEL_EDITION_FUNCS_H
#define VOXEL_EDITION_FUNCS_H

#include "../storage/funcs.h"
#include "../storage/materials_4i4w.h"
#include "../storage/voxel_data_grid.h"
#include "../util/containers/dynamic_bitset.h"
#include "../util/containers/fixed_array.h"
#include "../util/math/conv.h"
#include "../util/math/sdf.h"
#include "../util/math/transform_3d.h"
#include "../util/profiling.h"

namespace zylann::voxel {

// Interpolates values from a 3D grid at a given position, using trilinear interpolation.
// If the position is outside the grid, values are clamped.
inline float interpolate_trilinear(Span<const float> grid, const Vector3i res, const Vector3f pos) {
	const Vector3f pfi = math::floor(pos - Vector3f(0.5f));
	// TODO Clamp pf too somehow?
	const Vector3f pf = pos - pfi;
	const Vector3i max_pos = math::max(res - Vector3i(2, 2, 2), Vector3i());
	const Vector3i pi = math::clamp(Vector3i(pfi.x, pfi.y, pfi.z), Vector3i(), max_pos);

	const unsigned int n010 = 1;
	const unsigned int n100 = res.y;
	const unsigned int n001 = res.y * res.x;

	const unsigned int i000 = pi.x * n100 + pi.y * n010 + pi.z * n001;
	const unsigned int i010 = i000 + n010;
	const unsigned int i100 = i000 + n100;
	const unsigned int i001 = i000 + n001;
	const unsigned int i110 = i010 + n100;
	const unsigned int i011 = i010 + n001;
	const unsigned int i101 = i100 + n001;
	const unsigned int i111 = i110 + n001;

	return math::interpolate_trilinear(
			grid[i000], grid[i100], grid[i101], grid[i001], grid[i010], grid[i110], grid[i111], grid[i011], pf);
}

template <typename Volume_F>
float get_sdf_interpolated(const Volume_F &f, Vector3 pos) {
	const Vector3i c = math::floor_to_int(pos);

	const float s000 = f(Vector3i(c.x, c.y, c.z));
	const float s100 = f(Vector3i(c.x + 1, c.y, c.z));
	const float s010 = f(Vector3i(c.x, c.y + 1, c.z));
	const float s110 = f(Vector3i(c.x + 1, c.y + 1, c.z));
	const float s001 = f(Vector3i(c.x, c.y, c.z + 1));
	const float s101 = f(Vector3i(c.x + 1, c.y, c.z + 1));
	const float s011 = f(Vector3i(c.x, c.y + 1, c.z + 1));
	const float s111 = f(Vector3i(c.x + 1, c.y + 1, c.z + 1));

	return math::interpolate_trilinear(s000, s100, s101, s001, s010, s110, s111, s011, to_vec3f(math::fract(pos)));
}

// Standalone helper function to copy voxels from any 3D chunked container
void copy_from_chunked_storage( //
		VoxelBuffer &dst_buffer, //
		Vector3i min_pos, //
		unsigned int block_size_po2, //
		uint32_t channels_mask, //
		const VoxelBuffer *(*get_block_func)(void *, Vector3i), //
		void *get_block_func_ctx //
);

// Standalone helper function to paste voxels to any 3D chunked container
void paste_to_chunked_storage( //
		const VoxelBuffer &src_buffer, //
		Vector3i min_pos, //
		unsigned int block_size_po2, //
		unsigned int channels_mask, //
		bool use_mask, //
		uint8_t mask_channel, //
		uint64_t mask_value, //
		VoxelBuffer *(*get_block_func)(void *, Vector3i), //
		void *get_block_func_ctx //
);

template <typename FGetBlock, typename FPaste>
void paste_to_chunked_storage_tp( //
		const VoxelBuffer &src_buffer, //
		Vector3i min_pos, //
		unsigned int block_size_po2, //
		unsigned int channels_mask, //
		FGetBlock get_block_func, // (position) -> VoxelBuffer*
		FPaste paste_func // (channels, src_buffer, dst_buffer, dst_base_pos) -> void
) {
	const Vector3i max_pos = min_pos + src_buffer.get_size();

	const Vector3i min_block_pos = min_pos >> block_size_po2;
	const Vector3i max_block_pos = ((max_pos - Vector3i(1, 1, 1)) >> block_size_po2) + Vector3i(1, 1, 1);

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				VoxelBuffer *dst_buffer = get_block_func(bpos);

				if (dst_buffer == nullptr) {
					continue;
				}

				const Vector3i dst_block_origin = bpos << block_size_po2;
				const Vector3i dst_base_pos = min_pos - dst_block_origin;

				paste_func(to_span(channels), src_buffer, *dst_buffer, dst_base_pos);
			}
		}
	}
}

namespace paste_functors {

// Wraps calls to paste functions due to a few differences for now.

struct Default {
	void operator()(Span<const uint8_t> channels, const VoxelBuffer &src, VoxelBuffer &dst, Vector3i dst_base_pos) {
		paste(channels, src, dst, dst_base_pos, true);
	}
};

struct SrcMasked {
	uint8_t src_mask_channel;
	uint64_t src_mask_value;

	void operator()(Span<const uint8_t> channels, const VoxelBuffer &src, VoxelBuffer &dst, Vector3i dst_base_pos) {
		paste_src_masked(channels, src, src_mask_channel, src_mask_value, dst, dst_base_pos, true);
	}
};

struct SrcMasked_DstWritableValue {
	const uint8_t src_mask_channel;
	const uint8_t dst_mask_channel;
	const uint64_t src_mask_value;
	const uint64_t dst_mask_value;

	void operator()(Span<const uint8_t> channels, const VoxelBuffer &src, VoxelBuffer &dst, Vector3i dst_base_pos) {
		paste_src_masked_dst_writable_value( //
				channels, //
				src, //
				src_mask_channel, //
				src_mask_value, //
				dst, //
				dst_base_pos, //
				dst_mask_channel, //
				dst_mask_value, //
				true //
		);
	}
};

struct SrcMasked_DstWritableBitArray {
	const uint8_t src_mask_channel;
	const uint8_t dst_mask_channel;
	const uint64_t src_mask_value;
	const DynamicBitset &bitarray;

	void operator()(Span<const uint8_t> channels, const VoxelBuffer &src, VoxelBuffer &dst, Vector3i dst_base_pos) {
		paste_src_masked_dst_writable_bitarray( //
				channels, //
				src, //
				src_mask_channel, //
				src_mask_value, //
				dst, //
				dst_base_pos, //
				dst_mask_channel, //
				bitarray, //
				true //
		);
	}
};

} // namespace paste_functors

bool indices_to_bitarray_u16(Span<const int32_t> indices, DynamicBitset &bitarray);

template <typename FGetBlock>
void paste_to_chunked_storage_masked_writable_list( //
		const VoxelBuffer &src, //
		Vector3i min_pos, //
		uint8_t block_size_po2, //
		uint8_t channels_mask, //
		FGetBlock get_block_func, // (position) -> VoxelBuffer*
		uint8_t src_mask_channel, //
		uint64_t src_mask_value, //
		uint8_t dst_mask_channel, //
		Span<const int32_t> dst_writable_values //
) {
	using namespace paste_functors;

	if (dst_writable_values.size() == 1) {
		const uint64_t dst_mask_value = dst_writable_values[0];
		paste_to_chunked_storage_tp(src, min_pos, block_size_po2, channels_mask, //
				get_block_func, //
				SrcMasked_DstWritableValue{ src_mask_channel, dst_mask_channel, src_mask_value, dst_mask_value } //
		);

	} else {
		// TODO Candidate for TempAllocator
		DynamicBitset bitarray;
		indices_to_bitarray_u16(dst_writable_values, bitarray);
		paste_to_chunked_storage_tp(src, min_pos, block_size_po2, channels_mask, //
				get_block_func, //
				SrcMasked_DstWritableBitArray{ src_mask_channel, dst_mask_channel, src_mask_value, bitarray } //
		);
	}
}

AABB get_path_aabb(Span<const Vector3> positions, Span<const float> radii);

} // namespace zylann::voxel

// Library of templates for executing per-voxel operations.
// There is a bunch of compile-time abstraction boilerplate, which is to minimize the code to write when adding new
// operations, and have them work with different chunked containers, different edition modes, different formats... while
// also trying to avoid runtime per-voxel branching and checks for all these cases.
namespace zylann::voxel::ops {

// Operations

template <typename Op, typename Shape>
struct SdfOperation16bit {
	Op op;
	Shape shape;
	inline int16_t operator()(Vector3i pos, int16_t sdf) const {
		return snorm_to_s16(op(s16_to_snorm(sdf) * constants::QUANTIZED_SDF_16_BITS_SCALE_INV, shape(Vector3(pos))) *
				constants::QUANTIZED_SDF_16_BITS_SCALE);
	}
};

struct SdfUnion {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, math::sdf_union(a, b), strength);
	}
};

struct SdfSubtract {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, math::sdf_subtract(a, b), strength);
	}
};

struct SdfSet {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, b, strength);
	}
};

template <typename Value_T, typename Shape_T>
struct BlockySetOperation {
	Shape_T shape;
	Value_T value;
	inline Value_T operator()(Vector3i pos, Value_T src) const {
		return shape.is_inside(Vector3(pos)) ? value : src;
	}
};

inline Box3i get_sdf_sphere_box(Vector3 center, real_t radius) {
	return Box3i::from_min_max( //
			math::floor_to_int(center - Vector3(radius, radius, radius)),
			math::ceil_to_int(center + Vector3(radius, radius, radius)))
			// That padding is for SDF to have some margin
			// TODO Don't add padding from here, it must be done at higher level, where we know the type of operation
			.padded(2);
}

// Shapes

struct SdfSphere {
	Vector3 center;
	real_t radius;
	real_t sdf_scale;

	// TODO Rename get_signed_distance?
	inline real_t operator()(Vector3 pos) const {
		return sdf_scale * math::sdf_sphere(pos, center, radius);
	}

	inline bool is_inside(Vector3 pos) const {
		// Faster than the true SDF, we avoid a square root
		return center.distance_squared_to(pos) < radius * radius;
	}

	// TODO Seems unused?
	inline const char *name() const {
		return "SdfSphere";
	}

	inline Box3i get_box() {
		return get_sdf_sphere_box(center, radius);
	}
};

struct SdfHemisphere {
	Vector3 center;
	Vector3 flat_direction;
	real_t plane_d;
	real_t radius;
	real_t sdf_scale;
	real_t smoothness;

	inline real_t operator()(Vector3 pos) const {
		return sdf_scale *
				math::sdf_smooth_subtract( //
						math::sdf_sphere(pos, center, radius), //
						math::sdf_plane(pos, flat_direction, plane_d), smoothness);
	}

	inline bool is_inside(Vector3 pos) const {
		return (*this)(pos) < 0;
	}

	inline const char *name() const {
		return "SdfHemisphere";
	}

	inline Box3i get_box() {
		return get_sdf_sphere_box(center, radius);
	}
};

struct SdfBufferShape {
	Span<const float> buffer;
	Vector3i buffer_size;
	Transform3D world_to_buffer;
	float isolevel;
	float sdf_scale;

	inline real_t operator()(const Vector3 &wpos) const {
		// Transform terrain-space position to buffer-space
		const Vector3f lpos = to_vec3f(world_to_buffer.xform(wpos));
		if (lpos.x < 0 || lpos.y < 0 || lpos.z < 0 || lpos.x >= buffer_size.x || lpos.y >= buffer_size.y ||
				lpos.z >= buffer_size.z) {
			// Outside the buffer
			return constants::SDF_FAR_OUTSIDE;
		}
		// TODO Trilinear looks bad when the shape is scaled up.
		// Use Hermite in 3D https://www.researchgate.net/publication/360206102_Hermite_interpolation_of_heightmaps
		return interpolate_trilinear(buffer, buffer_size, lpos) * sdf_scale - isolevel;
	}

	inline const char *name() const {
		return "SdfBufferShape";
	}
};

struct SdfAxisAlignedBox {
	Vector3 center;
	Vector3 half_size;
	real_t sdf_scale;

	inline real_t operator()(Vector3 pos) const {
		return sdf_scale * math::sdf_box(pos - center, half_size);
	}

	inline bool is_inside(Vector3 pos) const {
		return AABB(center - half_size, 2 * half_size).has_point(pos);
	}

	inline Box3i get_box() {
		return Box3i::from_min_max( //
				math::floor_to_int(center - half_size), math::ceil_to_int(center + half_size))
				// That padding is for SDF to have some margin
				// TODO Don't add padding from here, it must be done at higher level, where we know the type of
				// operation
				.padded(2);
	}
};

Box3i get_round_cone_int_bounds(Vector3 p0, Vector3 p1, float r0, float r1);

struct SdfRoundCone {
	math::SdfRoundConePrecalc cone;
	real_t sdf_scale;

	inline real_t operator()(Vector3 pos) const {
		return sdf_scale * cone(pos);
	}

	inline bool is_inside(Vector3 pos) const {
		return cone(pos) < 0.f;
	}

	Box3i get_box() const {
		return get_round_cone_int_bounds(cone.a, cone.b, cone.r1, cone.r2);
	}
};

struct TextureParams {
	float opacity = 1.f;
	float sharpness = 2.f;
	unsigned int index = 0;
};

// Optimized for spheres
struct TextureBlendSphereOp {
	Vector3 center;
	float radius;
	float radius_squared;
	TextureParams tp;

	TextureBlendSphereOp(Vector3 p_center, float p_radius, TextureParams p_tp) {
		center = p_center;
		radius = p_radius;
		radius_squared = p_radius * p_radius;
		tp = p_tp;
	}

	inline void operator()(Vector3i pos, uint16_t &indices, uint16_t &weights) const {
		const float distance_squared = Vector3(pos).distance_squared_to(center);
		// Avoiding square root on the hot path
		if (distance_squared < radius_squared) {
			const float distance_from_radius = radius - Math::sqrt(distance_squared);
			const float target_weight =
					tp.opacity * math::clamp(tp.sharpness * (distance_from_radius / radius), 0.f, 1.f);
			blend_texture_packed_u16(tp.index, target_weight, indices, weights);
		}
	}
};

template <typename Shape_T>
struct TextureBlendOp {
	Shape_T shape;
	TextureParams texture_params;

	inline void operator()(Vector3i pos, uint16_t &indices, uint16_t &weights) const {
		const float sd = shape(pos);
		if (sd <= 0) {
			// TODO We don't know the full size of the shape so sharpness may be adjusted
			const float target_weight = texture_params.opacity * math::clamp(-sd * texture_params.sharpness, 0.f, 1.f);
			blend_texture_packed_u16(texture_params.index, target_weight, indices, weights);
		}
	}
};

enum Mode { //
	MODE_ADD,
	MODE_REMOVE,
	MODE_SET,
	MODE_TEXTURE_PAINT
};

// Single-value helper. Prefer using bulk APIs otherwise.
inline float sdf_blend(float src_value, float dst_value, Mode mode) {
	float res;
	switch (mode) {
		case MODE_ADD:
			res = zylann::math::sdf_union(src_value, dst_value);
			break;

		case MODE_REMOVE:
			// Relative complement (or difference)
			res = zylann::math::sdf_subtract(dst_value, src_value);
			break;

		case MODE_SET:
			res = src_value;
			break;

		default:
			res = 0;
			break;
	}
	return res;
}

// This one is implemented manually for a fast-path in texture paint.
// Also handles locking...
// TODO Find a nicer way to do this without copypasta
struct DoSphere {
	SdfSphere shape;
	Mode mode;
	VoxelDataGrid blocks;
	Box3i box;
	TextureParams texture_params;
	VoxelBuffer::ChannelId channel;
	uint32_t blocky_value;
	float strength;

	void operator()() {
		ZN_PROFILE_SCOPE();

		if (channel == VoxelBuffer::CHANNEL_SDF) {
			switch (mode) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume
					SdfOperation16bit<SdfUnion, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBuffer::CHANNEL_SDF, op);
				} break;

				case MODE_REMOVE: {
					SdfOperation16bit<SdfSubtract, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBuffer::CHANNEL_SDF, op);
				} break;

				case MODE_SET: {
					SdfOperation16bit<SdfSet, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBuffer::CHANNEL_SDF, op);
				} break;

				case MODE_TEXTURE_PAINT: {
					blocks.write_box_2(box, VoxelBuffer::CHANNEL_INDICES, VoxelBuffer::CHANNEL_WEIGHTS,
							TextureBlendSphereOp(shape.center, shape.radius, texture_params));
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}
		} else {
			BlockySetOperation<uint32_t, SdfSphere> op;
			op.shape = shape;
			op.value = blocky_value;
			blocks.write_box(box, channel, op);
		}
	}
};

template <typename TBlockAccess, typename FBlockAction>
void process_chunked_storage(Box3i voxel_box,
		// VoxelBuffer *get_block(Vector3i bpos)
		// unsigned int get_block_size_po2()
		TBlockAccess block_access,
		// void(VoxelBuffer &vb, Box3i local_box, Vector3i origin)
		FBlockAction op_func) {
	//
	const Vector3i max_pos = voxel_box.position + voxel_box.size;

	const unsigned int block_size_po2 = block_access.get_block_size_po2();

	const Vector3i min_block_pos = voxel_box.position >> block_size_po2;
	const Vector3i max_block_pos = ((max_pos - Vector3i(1, 1, 1)) >> block_size_po2) + Vector3i(1, 1, 1);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				VoxelBuffer *block = block_access.get_block(bpos);
				if (block == nullptr) {
					continue;
				}
				const Vector3i block_origin = bpos << block_size_po2;
				const Box3i local_box = Box3i(voxel_box.position - block_origin, voxel_box.size)
												.clipped(Box3i(Vector3i(), Vector3iUtil::create(1 << block_size_po2)));

				op_func(*block, local_box, block_origin);
			}
		}
	}
}

template <typename TBlockAccess, typename FOp>
inline void write_box_in_chunked_storage_1_channel(
		// D process(D src, Vector3i position)
		const FOp &op, TBlockAccess &block_access, Box3i box, VoxelBuffer::ChannelId channel_id) {
	//
	process_chunked_storage(
			box, block_access, [&op, channel_id](VoxelBuffer &vb, const Box3i local_box, Vector3i origin) {
				vb.write_box(local_box, channel_id, op, origin);
			});
}

template <typename TBlockAccess, typename FOp>
inline void write_box_in_chunked_storage_2_channels(
		// D process(D src, Vector3i position)
		const FOp &op, TBlockAccess &block_access, Box3i box, VoxelBuffer::ChannelId channel0_id,
		VoxelBuffer::ChannelId channel1_id) {
	//
	process_chunked_storage(box, block_access,
			[&op, channel0_id, channel1_id](VoxelBuffer &vb, const Box3i local_box, Vector3i origin) {
				vb.write_box_2_template<FOp, uint16_t, uint16_t>(local_box, channel0_id, channel1_id, op, origin);
			});
}

struct VoxelDataGridAccess {
	VoxelDataGrid *grid = nullptr;
	inline VoxelBuffer *get_block(const Vector3i &bpos) {
		return grid->get_block_no_lock(bpos);
	}
	inline unsigned int get_block_size_po2() const {
		return grid->get_block_size_po2();
	}
};

// Executes an operation in a box of a chunked voxel storage.
template <typename TShape, typename TBlockAccess>
struct DoShapeChunked {
	TShape shape;
	Mode mode;
	// VoxelBuffer *get_block(Vector3i bpos)
	// unsigned int get_block_size_po2()
	TBlockAccess block_access;
	Box3i box;
	VoxelBuffer::ChannelId channel;
	TextureParams texture_params;
	uint32_t blocky_value;
	float strength;

	void operator()() {
		ZN_PROFILE_SCOPE();

		if (channel == VoxelBuffer::CHANNEL_SDF) {
			switch (mode) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume. Or separate encoding?
					SdfOperation16bit<SdfUnion, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBuffer::CHANNEL_SDF);
				} break;

				case MODE_REMOVE: {
					SdfOperation16bit<SdfSubtract, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBuffer::CHANNEL_SDF);
				} break;

				case MODE_SET: {
					SdfOperation16bit<SdfSet, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBuffer::CHANNEL_SDF);
				} break;

				case MODE_TEXTURE_PAINT: {
					TextureBlendOp<TShape> op;
					op.shape = shape;
					op.texture_params = texture_params;
					write_box_in_chunked_storage_2_channels(
							op, block_access, box, VoxelBuffer::CHANNEL_INDICES, VoxelBuffer::CHANNEL_WEIGHTS);
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}

		} else {
			BlockySetOperation<uint32_t, TShape> op;
			op.shape = shape;
			op.value = blocky_value;
			write_box_in_chunked_storage_1_channel(op, block_access, box, channel);
		}
	}
};

template <typename TShape>
struct DoShapeSingleBuffer {
	TShape shape;
	Mode mode;
	VoxelBuffer *buffer = nullptr;
	Box3i box; // Should be clipped by the user
	VoxelBuffer::ChannelId channel;
	TextureParams texture_params;
	uint32_t blocky_value;
	float strength;

	void operator()() {
		ZN_PROFILE_SCOPE();
		ZN_ASSERT(buffer != nullptr);

		// const Box3i clipped_box = box.clip(Box3i(Vector3i(), buffer.get_size()));

		if (channel == VoxelBuffer::CHANNEL_SDF) {
			switch (mode) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume. Or separate encoding?
					SdfOperation16bit<SdfUnion, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					buffer->write_box(box, channel, op, Vector3i());
				} break;

				case MODE_REMOVE: {
					SdfOperation16bit<SdfSubtract, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					buffer->write_box(box, channel, op, Vector3i());
				} break;

				case MODE_SET: {
					SdfOperation16bit<SdfSet, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					buffer->write_box(box, channel, op, Vector3i());
				} break;

				case MODE_TEXTURE_PAINT: {
					TextureBlendOp<TShape> op;
					op.shape = shape;
					op.texture_params = texture_params;
					buffer->write_box_2_template<TextureBlendOp<TShape>, uint16_t, uint16_t>(
							box, VoxelBuffer::CHANNEL_INDICES, VoxelBuffer::CHANNEL_WEIGHTS, op, Vector3i());
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}

		} else {
			BlockySetOperation<uint32_t, TShape> op;
			op.shape = shape;
			op.value = blocky_value;
			buffer->write_box(box, channel, op, Vector3i());
		}
	}
};

#ifdef DEBUG_ENABLED
void box_blur_slow_ref(const VoxelBuffer &src, VoxelBuffer &dst, int radius, Vector3f sphere_pos, float sphere_radius);
#endif

void box_blur(const VoxelBuffer &src, VoxelBuffer &dst, int radius, Vector3f sphere_pos, float sphere_radius);

void grow_sphere(VoxelBuffer &src, float strength, Vector3f sphere_pos, float sphere_radius);

} // namespace zylann::voxel::ops

#endif // VOXEL_EDITION_FUNCS_H
