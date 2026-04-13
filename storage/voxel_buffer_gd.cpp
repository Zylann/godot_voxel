#include "voxel_buffer_gd.h"
#include "../edition/voxel_tool_buffer.h"
#include "../util/dstack.h"
#include "../util/godot/classes/image.h"
#include "../util/godot/classes/image_texture_3d.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/math/color.h"
#include "../util/memory/memory.h"
#include "../util/string/format.h"
#include "metadata/voxel_metadata_variant.h"

#ifdef ZN_GODOT
#include "../util/godot/core/class_db.h"
#endif

namespace zylann::voxel {

template <typename F>
void op_buffer_value_f(
		VoxelBuffer &dst,
		const float b,
		VoxelBuffer::ChannelId channel,
		F f // (float a, float b) -> float
) {
	if (dst.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
		const float a = dst.get_voxel_f(0, 0, 0, channel);
		dst.fill_f(f(a, b), channel);
		return;
	}

	switch (dst.get_channel_depth(channel)) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> dst_data;
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (int8_t &d : dst_data) {
				const float a = s8_to_snorm(d) * constants::QUANTIZED_SDF_8_BITS_SCALE_INV;
				d = snorm_to_s8(f(a, b) * constants::QUANTIZED_SDF_8_BITS_SCALE);
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> dst_data;
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (int16_t &d : dst_data) {
				const float a = s16_to_snorm(d) * constants::QUANTIZED_SDF_16_BITS_SCALE_INV;
				d = snorm_to_s16(f(a, b) * constants::QUANTIZED_SDF_16_BITS_SCALE);
			}
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<float> dst_data;
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (float &d : dst_data) {
				d = f(d, b);
			}
		} break;

		case VoxelBuffer::DEPTH_64_BIT:
			ZN_PRINT_ERROR("Unsupported depth for operation");
			break;

		default:
			ZN_CRASH();
			break;
	}
}

template <typename F>
void op_buffer_buffer_f(
		VoxelBuffer &dst,
		const VoxelBuffer &src,
		VoxelBuffer::ChannelId channel,
		F f // (float a, float b) -> float
) {
	if (src.get_channel_compression(channel) == zylann::voxel::VoxelBuffer::COMPRESSION_UNIFORM) {
		const float value = src.get_voxel_f(0, 0, 0, channel);
		op_buffer_value_f(dst, value, channel, f);
		return;
	}

	if (dst.get_channel_compression(channel) == zylann::voxel::VoxelBuffer::COMPRESSION_UNIFORM) {
		dst.decompress_channel(channel);
	}

	switch (src.get_channel_depth(channel)) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<const int8_t> src_data;
			Span<int8_t> dst_data;
			ZN_ASSERT(src.get_channel_data_read_only(channel, src_data));
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (unsigned int i = 0; i < src_data.size(); ++i) {
				const float a = s8_to_snorm(dst_data[i]) * constants::QUANTIZED_SDF_8_BITS_SCALE_INV;
				const float b = s8_to_snorm(src_data[i]) * constants::QUANTIZED_SDF_8_BITS_SCALE_INV;
				dst_data[i] = snorm_to_s8(f(a, b) * constants::QUANTIZED_SDF_8_BITS_SCALE);
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<const int16_t> src_data;
			Span<int16_t> dst_data;
			ZN_ASSERT(src.get_channel_data_read_only(channel, src_data));
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (unsigned int i = 0; i < src_data.size(); ++i) {
				const float a = s16_to_snorm(dst_data[i]) * constants::QUANTIZED_SDF_16_BITS_SCALE_INV;
				const float b = s16_to_snorm(src_data[i]) * constants::QUANTIZED_SDF_16_BITS_SCALE_INV;
				dst_data[i] = snorm_to_s16(f(a, b) * constants::QUANTIZED_SDF_16_BITS_SCALE);
			}
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<const float> src_data;
			Span<float> dst_data;
			ZN_ASSERT(src.get_channel_data_read_only(channel, src_data));
			ZN_ASSERT(dst.get_channel_data(channel, dst_data));
			for (unsigned int i = 0; i < src_data.size(); ++i) {
				dst_data[i] = f(dst_data[i], src_data[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_64_BIT:
			ZN_PRINT_ERROR("Non-implemented depth for operation");
			break;

		default:
			ZN_CRASH();
			break;
	}
}

// Converts the SDF channel into a 3D texture. If the output format is R8 or L8, pixels will contain a normalized
// distance field (so in shader, 0.5 will be the isolevel instead of 0, and values will go from 0 to 1). Note: in
// shader, you should use the `yxz` swizzle to sample pixels of that texture, because this is how voxels are stored and
// this function does not convert the coordinate system.
TypedArray<Image> sdf_to_3d_texture_data_zxy(const VoxelBuffer &vb, const Image::Format output_format) {
	ZN_PROFILE_SCOPE();

	const VoxelBuffer::ChannelId channel = zylann::voxel::VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::Depth depth = vb.get_channel_depth(channel);
	const uint64_t xy_area = vb.get_size().x * vb.get_size().y;
	const VoxelBuffer::Compression channel_compression = vb.get_channel_compression(channel);

	// TODO An array of images is going to waste resources... Godot should really have an Image3D class, or just allow
	// to pass raw data... `Image` is more than 400 Kb, which is more than a single slice of 8-bit pixels in a 16x16x16
	// chunk!
	TypedArray<Image> images;
	images.resize(vb.get_size().z);

	// TODO Is it possible for a shader to specify that a 8-bit depth texture should be sampled as inorm8 and not
	// unorm8? Because if not, we have to do extra work to convert every voxel here.
	// That's why for now we have to do these conversions
	struct L {
		static inline uint8_t s8_to_u8(const int8_t i) {
			return static_cast<uint8_t>(static_cast<int16_t>(i) + 128);
		}

		static inline uint8_t s16_to_u8(const int16_t i) {
			return static_cast<uint16_t>((static_cast<int32_t>(i) + 32768) >> 8);
		}
	};

	// Not all combinations are supported. For now we only implement those we need.
	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			ZN_PRINT_ERROR("Channel depth not supported.");
			return TypedArray<Image>();

		case VoxelBuffer::DEPTH_16_BIT: {
			switch (output_format) {
				case Image::FORMAT_R8:
				case Image::FORMAT_L8: {
					// Get fixed-point signed normalized 16-bit SDF to fixed-point unsigned normalized 8-bit SDF.
					// Sampling this in shader may need something like `(texture(t, pos).r * 2.0 - 1.0) * scale`

					if (channel_compression == VoxelBuffer::COMPRESSION_UNIFORM) {
						const int16_t sd_s16 = vb.get_voxel(Vector3i(0, 0, 0), channel);
						const uint8_t sd_u8 = L::s16_to_u8(sd_s16);

						for (int z = 0; z < vb.get_size().z; ++z) {
							PackedByteArray pba;
							pba.resize(xy_area);
							pba.fill(sd_u8);
							Ref<Image> image = Image::create_from_data(
									vb.get_size().y, vb.get_size().x, false, output_format, pba
							);
							images[z] = image;
						}
					} else {
						Span<const int16_t> data;
						ZN_ASSERT_RETURN_V(vb.get_channel_data_read_only(channel, data), TypedArray<Image>());

						for (int z = 0; z < vb.get_size().z; ++z) {
							PackedByteArray pba;
							pba.resize(xy_area);
							Span<uint8_t> pba_s(pba.ptrw(), pba.size());

							Span<const int16_t> data_slice = data.sub(z * xy_area, xy_area);

							for (unsigned int i = 0; i < xy_area; ++i) {
								pba_s[i] = L::s16_to_u8(data_slice[i]);
							}

							Ref<Image> image = Image::create_from_data(
									vb.get_size().y, vb.get_size().x, false, output_format, pba
							);
							images[z] = image;
						}
					}
				} break;

				default:
					ZN_PRINT_ERROR("Image format not supported.");
					return TypedArray<Image>();
			}

		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			ZN_PRINT_ERROR("Channel depth not supported.");
			return TypedArray<Image>();
		} break;

		default:
			ZN_PRINT_ERROR("Channel depth not supported.");
			return TypedArray<Image>();
	}

	return images;
}

Ref<ImageTexture3D> create_3d_texture_from_sdf_zxy(const VoxelBuffer &vb, const Image::Format output_format) {
	TypedArray<Image> images = sdf_to_3d_texture_data_zxy(vb, output_format);
	Ref<ImageTexture3D> texture = zylann::godot::create_image_texture_3d(output_format, vb.get_size(), false, images);
	return texture;
}

void update_3d_texture_from_sdf_zxy(const VoxelBuffer &vb, ImageTexture3D &texture) {
	TypedArray<Image> images = sdf_to_3d_texture_data_zxy(vb, texture.get_format());
	// Format and size must match
	zylann::godot::update_image_texture_3d(texture, images);
}

PackedByteArray get_channel_as_byte_array(const VoxelBuffer &vb, const VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN_V(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS, PackedByteArray());

	const Vector3i res = vb.get_size();
	const uint64_t volume = Vector3iUtil::get_volume_u64(res);
	const VoxelBuffer::Compression compression = vb.get_channel_compression(channel);
	const VoxelBuffer::Depth depth = vb.get_channel_depth(channel);

	PackedByteArray pba;

	switch (compression) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			// Decompress... can't just decompress the VoxelBuffer directly with existing methods, because it is const,
			// and would waste intermediary memory.
			// If this behavior is not desired, the caller must check compression first.
			switch (depth) {
				case VoxelBuffer::DEPTH_8_BIT: {
					pba.resize(volume);
					const uint8_t v = vb.get_voxel(Vector3i(0, 0, 0), channel);
					pba.fill(v);
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					pba.resize(volume * sizeof(uint16_t));
					const uint16_t v = vb.get_voxel(Vector3i(0, 0, 0), channel);
					Span<uint16_t> pba_s = Span<uint8_t>(pba.ptrw(), pba.size()).reinterpret_cast_to<uint16_t>();
					pba_s.fill(v);
				} break;

				case VoxelBuffer::DEPTH_32_BIT: {
					pba.resize(volume * sizeof(uint32_t));
					const uint32_t v = vb.get_voxel(Vector3i(0, 0, 0), channel);
					Span<uint32_t> pba_s = Span<uint8_t>(pba.ptrw(), pba.size()).reinterpret_cast_to<uint32_t>();
					pba_s.fill(v);
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled channel depth");
					break;
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			Span<const uint8_t> src;
			ZN_ASSERT_RETURN_V(vb.get_channel_as_bytes_read_only(channel, src), pba);
			zylann::godot::copy_to(pba, src);
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return pba;
}

} // namespace zylann::voxel

namespace zylann::voxel::godot {

const char *VoxelBuffer::CHANNEL_ID_HINT_STRING = "Type,Sdf,Color,Indices,Weights,Data5,Data6,Data7";
static thread_local bool s_create_shared = false;

Variant get_voxel_metadata(const zylann::voxel::VoxelBuffer &vb, const Vector3i pos) {
	const VoxelMetadata *meta = vb.get_voxel_metadata(pos);
	if (meta == nullptr) {
		return Variant();
	}
	return get_as_variant(*meta);
}

void set_voxel_metadata(zylann::voxel::VoxelBuffer &vb, const Vector3i pos, const Variant &meta) {
	if (meta.get_type() == Variant::NIL) {
		vb.erase_voxel_metadata(pos);
	} else {
		VoxelMetadata *mv = vb.get_or_create_voxel_metadata(pos);
		ZN_ASSERT_RETURN(mv != nullptr);
		set_as_variant(*mv, meta);
	}
}

VoxelBuffer::VoxelBuffer() {
	if (!s_create_shared) {
		_buffer = make_shared_instance<zylann::voxel::VoxelBuffer>(zylann::voxel::VoxelBuffer::ALLOCATOR_DEFAULT);
	}
}

VoxelBuffer::VoxelBuffer(VoxelBuffer::Allocator allocator) {
	if (allocator < 0 || allocator >= ALLOCATOR_COUNT) {
		ZN_PRINT_ERROR(format("Out of bounds allocator {}", allocator));
		allocator = ALLOCATOR_DEFAULT;
	}
	_buffer = make_shared_instance<zylann::voxel::VoxelBuffer>(
			static_cast<zylann::voxel::VoxelBuffer::Allocator>(allocator)
	);
}

VoxelBuffer::VoxelBuffer(std::shared_ptr<zylann::voxel::VoxelBuffer> &other) {
	CRASH_COND(other == nullptr);
	_buffer = other;
}

void VoxelBuffer::create(int x, int y, int z) {
	ZN_ASSERT_RETURN(x >= 0);
	ZN_ASSERT_RETURN(y >= 0);
	ZN_ASSERT_RETURN(z >= 0);
	// Not exposing allocators to scripts for now. Will do if the need comes up.
	// ZN_ASSERT_RETURN(allocator >= 0 && allocator < ALLOCATOR_COUNT);
	// _buffer->create(Vector3i(x, y, z), static_cast<zylann::voxel::VoxelBuffer::Allocator>(allocator));
	_buffer->create(Vector3i(x, y, z));
}

Ref<VoxelBuffer> VoxelBuffer::create_shared(std::shared_ptr<zylann::voxel::VoxelBuffer> &other) {
	Ref<VoxelBuffer> vb;
	s_create_shared = true;
	vb.instantiate();
	s_create_shared = false;
	vb->_buffer = other;
	return vb;
}

VoxelBuffer::~VoxelBuffer() {}

void VoxelBuffer::clear() {
	_buffer->clear();
}

real_t VoxelBuffer::get_voxel_f(int x, int y, int z, unsigned int channel_index) const {
	return _buffer->get_voxel_f(x, y, z, channel_index);
}

void VoxelBuffer::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	ZN_DSTACK();
	return _buffer->set_voxel_f(value, x, y, z, channel_index);
}

void VoxelBuffer::copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel) {
	ZN_DSTACK();
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_channel_from(other->get_buffer(), channel);
}

void VoxelBuffer::copy_channel_from_area(
		Ref<VoxelBuffer> other,
		Vector3i src_min,
		Vector3i src_max,
		Vector3i dst_min,
		unsigned int channel
) {
	ZN_DSTACK();
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_channel_from(other->get_buffer(), src_min, src_max, dst_min, channel);
}

void VoxelBuffer::fill(uint64_t defval, int channel_index) {
	ZN_DSTACK();
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	_buffer->fill(defval, channel_index);
}

void VoxelBuffer::fill_f(real_t value, int channel) {
	ZN_DSTACK();
	ERR_FAIL_INDEX(channel, MAX_CHANNELS);
	_buffer->fill_f(value, channel);
}

bool VoxelBuffer::is_uniform(int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);
	return _buffer->is_uniform(channel_index);
}

void VoxelBuffer::compress_uniform_channels() {
	_buffer->compress_uniform_channels();
}

VoxelBuffer::Compression VoxelBuffer::get_channel_compression(int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, VoxelBuffer::COMPRESSION_NONE);
	return VoxelBuffer::Compression(_buffer->get_channel_compression(channel_index));
}

void VoxelBuffer::decompress_channel(int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	return _buffer->decompress_channel(channel_index);
}

void VoxelBuffer::downscale_to(Ref<VoxelBuffer> dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const {
	ZN_DSTACK();
	ERR_FAIL_COND(dst.is_null());
	_buffer->downscale_to(dst->get_buffer(), src_min, src_max, dst_min);
}

void VoxelBuffer::rotate_90(Vector3i::Axis axis, int turns) {
	_buffer->transform(math::OrthoBasis::from_axis_turns(axis, turns));
}

static math::OrthoBasis mirror_axis_to_basis(const Vector3i::Axis axis) {
	switch (axis) {
		case Vector3i::AXIS_X:
			return math::OrthoBasis(Vector3i(-1, 0, 0), Vector3i(0, 1, 0), Vector3i(0, 0, 1));
		case Vector3i::AXIS_Y:
			return math::OrthoBasis(Vector3i(1, 0, 0), Vector3i(0, -1, 0), Vector3i(0, 0, 1));
		case Vector3i::AXIS_Z:
			return math::OrthoBasis(Vector3i(1, 0, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1));
		default:
			ZN_PRINT_ERROR("Invalid axis");
			return math::OrthoBasis();
	}
}

void VoxelBuffer::mirror(Vector3i::Axis axis) {
	_buffer->transform(mirror_axis_to_basis(axis));
}

Ref<VoxelBuffer> VoxelBuffer::duplicate(bool include_metadata) const {
	Ref<VoxelBuffer> d;
	d.instantiate();
	_buffer->copy_to(d->get_buffer(), include_metadata);
	return d;
}

Ref<VoxelTool> VoxelBuffer::get_voxel_tool() {
	// I can't make this function `const`, because `Ref<T>` has no constructor taking a `const T*`.
	// The compiler would then choose Ref<T>(const Variant&), which fumbles `this` into a null pointer
	Ref<VoxelBuffer> vb(this);
	return Ref<VoxelTool>(memnew(VoxelToolBuffer(vb)));
}

void VoxelBuffer::set_channel_depth(unsigned int channel_index, Depth new_depth) {
	_buffer->set_channel_depth(channel_index, zylann::voxel::VoxelBuffer::Depth(new_depth));
}

VoxelBuffer::Depth VoxelBuffer::get_channel_depth(unsigned int channel_index) const {
	return VoxelBuffer::Depth(_buffer->get_channel_depth(channel_index));
}

void VoxelBuffer::remap_values(unsigned int channel_index, PackedInt32Array map) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

	Span<const int> map_r(map.ptr(), map.size());
	const zylann::voxel::VoxelBuffer::Depth depth = _buffer->get_channel_depth(channel_index);

	// TODO If `get_channel_data` could return a span of size 1 for this case, we wouldn't need this code
	if (_buffer->get_channel_compression(channel_index) == zylann::voxel::VoxelBuffer::COMPRESSION_UNIFORM) {
		uint64_t v = _buffer->get_voxel(Vector3i(), channel_index);
		if (v < map_r.size()) {
			v = map_r[v];
		}
		_buffer->fill(v, channel_index);
		return;
	}

	switch (depth) {
		case zylann::voxel::VoxelBuffer::DEPTH_8_BIT: {
			Span<uint8_t> values;
			ZN_ASSERT_RETURN(_buffer->get_channel_as_bytes(channel_index, values));
			for (uint8_t &v : values) {
				if (v < map_r.size()) {
					v = map_r[v];
				}
			}
		} break;

		case zylann::voxel::VoxelBuffer::DEPTH_16_BIT: {
			Span<uint16_t> values;
			ZN_ASSERT_RETURN(_buffer->get_channel_data(channel_index, values));
			for (uint16_t &v : values) {
				if (v < map_r.size()) {
					v = map_r[v];
				}
			}
		} break;

		default:
			ZN_PRINT_ERROR("Remapping channel values is not implemented for depths greater than 16 bits.");
			break;
	}
}

VoxelBuffer::Allocator VoxelBuffer::get_allocator() const {
	return static_cast<VoxelBuffer::Allocator>(_buffer->get_allocator());
}

void VoxelBuffer::op_add_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(other.is_valid());
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_channel_depth(channel) == other->get_channel_depth(channel));
	ZN_ASSERT_RETURN(get_size() == other->get_size());
	op_buffer_buffer_f(
			*_buffer,
			other->get_buffer(),
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel),
			[](float a, float b) { return a + b; }
	);
}

void VoxelBuffer::op_sub_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(other.is_valid());
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_channel_depth(channel) == other->get_channel_depth(channel));
	ZN_ASSERT_RETURN(get_size() == other->get_size());
	op_buffer_buffer_f(
			*_buffer,
			other->get_buffer(),
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel),
			[](float a, float b) { return a - b; }
	);
}

void VoxelBuffer::op_mul_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(other.is_valid());
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_channel_depth(channel) == other->get_channel_depth(channel));
	ZN_ASSERT_RETURN(get_size() == other->get_size());
	op_buffer_buffer_f(
			*_buffer,
			other->get_buffer(),
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel),
			[](float a, float b) { return a * b; }
	);
}

void VoxelBuffer::op_mul_value_f(float scale, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	op_buffer_value_f(
			*_buffer,
			scale,
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel), //
			[](float a, float b) { return a * b; }
	);
}

void VoxelBuffer::op_min_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(other.is_valid());
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_channel_depth(channel) == other->get_channel_depth(channel));
	ZN_ASSERT_RETURN(get_size() == other->get_size());
	op_buffer_buffer_f(
			*_buffer,
			other->get_buffer(),
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel),
			[](float a, float b) { return math::min(a, b); }
	);
}

void VoxelBuffer::op_max_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel) {
	ZN_ASSERT_RETURN(other.is_valid());
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_channel_depth(channel) == other->get_channel_depth(channel));
	ZN_ASSERT_RETURN(get_size() == other->get_size());
	op_buffer_buffer_f(
			*_buffer,
			other->get_buffer(),
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel),
			[](float a, float b) { return math::max(a, b); }
	);
}

template <typename TIn, typename TOut>
inline TOut select_less(TIn src, TIn threshold, TOut value_if_less, TOut value_if_more) {
	return src < threshold ? value_if_less : value_if_more;
}

void VoxelBuffer::op_select_less_src_f_dst_i_values(
		Ref<VoxelBuffer> src_ref,
		const VoxelBuffer::ChannelId src_channel,
		const float threshold,
		const int value_if_less,
		const int value_if_more,
		const VoxelBuffer::ChannelId dst_channel
) {
	ZN_ASSERT_RETURN(src_ref.is_valid());
	ZN_ASSERT_RETURN(src_channel >= 0 && src_channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(dst_channel >= 0 && dst_channel < VoxelBuffer::MAX_CHANNELS);
	ZN_ASSERT_RETURN(get_size() == src_ref->get_size());

	const zylann::voxel::VoxelBuffer &src = src_ref->get_buffer();
	zylann::voxel::VoxelBuffer &dst = *_buffer;

	// Optimizable, but a bit too many combinations of formats than it's worth.
	// If necessary, only optimize common formats.

	if (src.get_channel_depth(src_channel) == zylann::voxel::VoxelBuffer::DEPTH_32_BIT &&
		dst.get_channel_depth(dst_channel) == zylann::voxel::VoxelBuffer::DEPTH_16_BIT) {
		//
		const uint16_t value_if_less_16 = math::clamp(value_if_less, 0, 65535);
		const uint16_t value_if_more_16 = math::clamp(value_if_more, 0, 65535);

		if (src.get_channel_compression(src_channel) == zylann::voxel::VoxelBuffer::COMPRESSION_UNIFORM) {
			const float src_v = src.get_voxel_f(0, 0, 0, src_channel);
			const int16_t dst_v = select_less(src_v, threshold, value_if_less, value_if_more);
			dst.fill(dst_v, dst_channel);

		} else {
			dst.decompress_channel(dst_channel);

			Span<const float> src_data;
			src.get_channel_data_read_only(src_channel, src_data);

			Span<uint16_t> dst_data;
			dst.get_channel_data(dst_channel, dst_data);

			for (unsigned int i = 0; i < src_data.size(); ++i) {
				dst_data[i] = select_less(src_data[i], threshold, value_if_less_16, value_if_more_16);
			}
		}

	} else {
		// Generic, slower version
		Vector3i pos;
		const Vector3i size = get_size();
		for (pos.z = 0; pos.z < size.z; ++pos.z) {
			for (pos.x = 0; pos.x < size.x; ++pos.x) {
				for (pos.y = 0; pos.y < size.y; ++pos.y) {
					const float sd = src.get_voxel_f(pos, src_channel);
					const int16_t dst_v = select_less(sd, threshold, value_if_less, value_if_more);
					dst.set_voxel(dst_v, pos, dst_channel);
				}
			}
		}
	}
}

Variant VoxelBuffer::get_block_metadata() const {
	return get_as_variant(_buffer->get_block_metadata());
}

void VoxelBuffer::set_block_metadata(Variant meta) {
	set_as_variant(_buffer->get_block_metadata(), meta);
}

Variant VoxelBuffer::get_voxel_metadata(Vector3i pos) const {
	return zylann::voxel::godot::get_voxel_metadata(*_buffer, pos);
}

void VoxelBuffer::set_voxel_metadata(Vector3i pos, Variant meta) {
	zylann::voxel::godot::set_voxel_metadata(*_buffer, pos, meta);
}

void VoxelBuffer::for_each_voxel_metadata(const Callable &callback) const {
	ERR_FAIL_COND(callback.is_null());
	//_buffer->for_each_voxel_metadata(callback);

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &metadata_map = _buffer->get_voxel_metadata();

	for (auto it = metadata_map.begin(); it != metadata_map.end(); ++it) {
		Variant v = get_as_variant(it->value);

#if defined(ZN_GODOT)
		// TODO Use template version? Could get closer to GodotCpp
		const Variant key = it->key;
		const Variant *args[2] = { &key, &v };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.callp(args, 2, retval, err);
		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key))
		);

#elif defined(ZN_GODOT_EXTENSION)
		// TODO Error reporting? GodotCpp doesn't expose anything
		// callback.call(it->key, v);
		// TODO GodotCpp is missing the implementation of `Callable::call`.
		ZN_PRINT_ERROR("Unable to call Callable, go moan at https://github.com/godotengine/godot-cpp/issues/802");
#endif
	}
}

void VoxelBuffer::for_each_voxel_metadata_in_area(const Callable &callback, Vector3i min_pos, Vector3i max_pos) {
	ERR_FAIL_COND(callback.is_null());

	const Box3i box = Box3i::from_min_max(min_pos, max_pos);

	_buffer->for_each_voxel_metadata_in_area(box, [&callback](Vector3i rel_pos, const VoxelMetadata &meta) {
		Variant v = get_as_variant(meta);

#if defined(ZN_GODOT)
		// TODO Use template version? Could get closer to GodotCpp
		const Variant key = rel_pos;
		const Variant *args[2] = { &key, &v };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.callp(args, 2, retval, err);
		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key))
		);

#elif defined(ZN_GODOT_EXTENSION)
		// Can't do error-reporting the same way we do in modules.
		callback.call(rel_pos, v);
#endif
	});
}

void VoxelBuffer::copy_voxel_metadata_in_area(
		Ref<VoxelBuffer> src_buffer,
		Vector3i src_min_pos,
		Vector3i src_max_pos,
		Vector3i dst_pos
) {
	ERR_FAIL_COND(src_buffer.is_null());
	_buffer->copy_voxel_metadata_in_area(
			src_buffer->get_buffer(), Box3i::from_min_max(src_min_pos, src_max_pos), dst_pos
	);
}

void VoxelBuffer::clear_voxel_metadata_in_area(Vector3i min_pos, Vector3i max_pos) {
	_buffer->clear_voxel_metadata_in_area(Box3i::from_min_max(min_pos, max_pos));
}

void VoxelBuffer::clear_voxel_metadata() {
	_buffer->clear_voxel_metadata();
}

Ref<ImageTexture3D> VoxelBuffer::create_3d_texture_from_sdf_zxy(const Image::Format output_format) const {
	return zylann::voxel::create_3d_texture_from_sdf_zxy(*_buffer, output_format);
}

void VoxelBuffer::update_3d_texture_from_sdf_zxy(Ref<ImageTexture3D> texture) const {
	ZN_ASSERT_RETURN(texture.is_valid());
	return zylann::voxel::update_3d_texture_from_sdf_zxy(*_buffer, **texture);
}

PackedByteArray VoxelBuffer::get_channel_as_byte_array(const ChannelId channel) const {
	return zylann::voxel::get_channel_as_byte_array(
			*_buffer, static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel)
	);
}

void VoxelBuffer::set_channel_from_byte_array(const ChannelId channel, const PackedByteArray &pba) {
	ZN_ASSERT_RETURN(channel >= 0 && channel < MAX_CHANNELS);
	_buffer->set_channel_from_bytes(channel, to_span(pba));
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down() {
	return debug_print_sdf_to_image_top_down(*_buffer);
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down(const zylann::voxel::VoxelBuffer &vb) {
	const Vector3i size = vb.get_size();
	Ref<Image> im = zylann::godot::create_empty_image(size.x, size.z, false, Image::FORMAT_RGB8);
	Vector3i pos;
	for (pos.z = 0; pos.z < size.z; ++pos.z) {
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = size.y - 1; pos.y >= 0; --pos.y) {
				float v = vb.get_voxel_f(pos.x, pos.y, pos.z, zylann::voxel::VoxelBuffer::CHANNEL_SDF);
				if (v < 0.0) {
					break;
				}
			}
			float h = pos.y;
			float c = h / size.y;
			im->set_pixel(pos.x, pos.z, Color(c, c, c));
		}
	}
	return im;
}

Ref<Image> VoxelBuffer::debug_print_sdf_y_slice(const zylann::voxel::VoxelBuffer &buffer, float scale, int y) {
	const Vector3i res = buffer.get_size();
	ERR_FAIL_COND_V(y < 0 || y >= res.y, Ref<Image>());

	Ref<Image> im = zylann::godot::create_empty_image(res.x, res.z, false, Image::FORMAT_RGB8);

	const Color nega_col(0.5f, 0.5f, 1.0f);
	const Color posi_col(1.0f, 0.6f, 0.1f);
	const Color black(0.f, 0.f, 0.f);

	for (int z = 0; z < res.z; ++z) {
		for (int x = 0; x < res.x; ++x) {
			const float sd = scale * buffer.get_voxel_f(x, y, z, zylann::voxel::VoxelBuffer::CHANNEL_SDF);

			const float nega = math::clamp(-sd, 0.0f, 1.0f);
			const float posi = math::clamp(sd, 0.0f, 1.0f);
			const Color col = math::lerp(black, nega_col, nega) + math::lerp(black, posi_col, posi);

			im->set_pixel(x, z, col);
		}
	}

	return im;
}

Ref<Image> VoxelBuffer::debug_print_sdf_z_slice(const zylann::voxel::VoxelBuffer &buffer, float scale, int z) {
	const Vector3i res = buffer.get_size();
	ERR_FAIL_COND_V(z < 0 || z >= res.z, Ref<Image>());

	Ref<Image> im = zylann::godot::create_empty_image(res.x, res.y, false, Image::FORMAT_RGB8);

	const Color nega_col(0.5f, 0.5f, 1.0f);
	const Color posi_col(1.0f, 0.6f, 0.1f);
	const Color black(0.f, 0.f, 0.f);

	for (int x = 0; x < res.x; ++x) {
		for (int y = 0; y < res.y; ++y) {
			const float sd = scale * buffer.get_voxel_f(x, y, z, zylann::voxel::VoxelBuffer::CHANNEL_SDF);

			const float nega = math::clamp(-sd, 0.0f, 1.0f);
			const float posi = math::clamp(sd, 0.0f, 1.0f);
			const Color col = math::lerp(black, nega_col, nega) + math::lerp(black, posi_col, posi);

			im->set_pixel(x, res.y - y - 1, col);
		}
	}

	return im;
}

Ref<Image> VoxelBuffer::debug_print_sdf_y_slice(float scale, int y) const {
	return debug_print_sdf_y_slice(*_buffer, scale, y);
}

TypedArray<Image> VoxelBuffer::debug_print_sdf_y_slices(float scale) const {
	TypedArray<Image> images;

	const zylann::voxel::VoxelBuffer &buffer = *_buffer;
	const Vector3i res = buffer.get_size();

	for (int y = 0; y < res.y; ++y) {
		images.append(debug_print_sdf_y_slice(buffer, scale, y));
	}

	return images;
}

void VoxelBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "sx", "sy", "sz"), &VoxelBuffer::_b_create);

	ClassDB::bind_method(D_METHOD("get_allocator"), &VoxelBuffer::get_allocator);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBuffer::clear);

	ClassDB::bind_method(D_METHOD("get_size"), &VoxelBuffer::get_size);

	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel, DEFVAL(0));
	ClassDB::bind_method(
			D_METHOD("set_voxel_f", "value", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel_f, DEFVAL(0)
	);
	ClassDB::bind_method(D_METHOD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::set_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_f", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_v", "pos", "channel"), &VoxelBuffer::get_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelBuffer::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("get_channel_depth", "channel"), &VoxelBuffer::get_channel_depth);
	ClassDB::bind_method(D_METHOD("set_channel_depth", "channel", "depth"), &VoxelBuffer::set_channel_depth);

	ClassDB::bind_method(D_METHOD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_f", "value", "channel"), &VoxelBuffer::fill_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area", "value", "min", "max", "channel"), &VoxelBuffer::fill_area, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area_f", "value", "min", "max", "channel"), &VoxelBuffer::fill_area_f);
	ClassDB::bind_method(D_METHOD("copy_channel_from", "other", "channel"), &VoxelBuffer::copy_channel_from);
	ClassDB::bind_method(
			D_METHOD("copy_channel_from_area", "other", "src_min", "src_max", "dst_min", "channel"),
			&VoxelBuffer::copy_channel_from_area
	);
	ClassDB::bind_method(D_METHOD("downscale_to", "dst", "src_min", "src_max", "dst_min"), &VoxelBuffer::downscale_to);
	ClassDB::bind_method(D_METHOD("rotate_90", "axis", "turns"), &VoxelBuffer::rotate_90);
	ClassDB::bind_method(D_METHOD("mirror", "axis"), &VoxelBuffer::mirror);

	ClassDB::bind_method(D_METHOD("is_uniform", "channel"), &VoxelBuffer::is_uniform);
	ClassDB::bind_method(D_METHOD("compress_uniform_channels"), &VoxelBuffer::compress_uniform_channels);
	ClassDB::bind_method(D_METHOD("get_channel_compression", "channel"), &VoxelBuffer::get_channel_compression);
	ClassDB::bind_method(D_METHOD("decompress_channel", "channel"), &VoxelBuffer::decompress_channel);

	ClassDB::bind_method(D_METHOD("remap_values", "channel", "map"), &VoxelBuffer::remap_values);

	ClassDB::bind_method(D_METHOD("op_add_buffer_f", "other", "channel"), &VoxelBuffer::op_add_buffer_f);
	ClassDB::bind_method(D_METHOD("op_sub_buffer_f", "other", "channel"), &VoxelBuffer::op_sub_buffer_f);
	ClassDB::bind_method(D_METHOD("op_mul_buffer_f", "other", "channel"), &VoxelBuffer::op_mul_buffer_f);
	ClassDB::bind_method(D_METHOD("op_mul_value_f", "other", "channel"), &VoxelBuffer::op_mul_value_f);
	ClassDB::bind_method(D_METHOD("op_min_buffer_f", "other", "channel"), &VoxelBuffer::op_min_buffer_f);
	ClassDB::bind_method(D_METHOD("op_max_buffer_f", "other", "channel"), &VoxelBuffer::op_max_buffer_f);
	ClassDB::bind_method(
			D_METHOD(
					"op_select_less_src_f_dst_i_values",
					"src",
					"src_channel",
					"threshold",
					"value_if_less",
					"value_if_more",
					"dst_channel"
			),
			&VoxelBuffer::op_select_less_src_f_dst_i_values
	);

	ClassDB::bind_method(D_METHOD("get_block_metadata"), &VoxelBuffer::get_block_metadata);
	ClassDB::bind_method(D_METHOD("set_block_metadata", "meta"), &VoxelBuffer::set_block_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelBuffer::get_voxel_metadata);
	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "value"), &VoxelBuffer::set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata", "callback"), &VoxelBuffer::for_each_voxel_metadata);
	ClassDB::bind_method(
			D_METHOD("for_each_voxel_metadata_in_area", "callback", "min_pos", "max_pos"),
			&VoxelBuffer::for_each_voxel_metadata_in_area
	);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata"), &VoxelBuffer::clear_voxel_metadata);
	ClassDB::bind_method(
			D_METHOD("clear_voxel_metadata_in_area", "min_pos", "max_pos"), &VoxelBuffer::clear_voxel_metadata_in_area
	);
	ClassDB::bind_method(
			D_METHOD("copy_voxel_metadata_in_area", "src_buffer", "src_min_pos", "src_max_pos", "dst_min_pos"),
			&VoxelBuffer::copy_voxel_metadata_in_area
	);
	ClassDB::bind_method(
			D_METHOD("create_3d_texture_from_sdf_zxy", "output_format"), &VoxelBuffer::create_3d_texture_from_sdf_zxy
	);
	ClassDB::bind_method(
			D_METHOD("update_3d_texture_from_sdf_zxy", "existing_texture"), &VoxelBuffer::update_3d_texture_from_sdf_zxy
	);
	ClassDB::bind_method(
			D_METHOD("get_channel_as_byte_array", "channel_index"), &VoxelBuffer::get_channel_as_byte_array
	);
	ClassDB::bind_method(
			D_METHOD("set_channel_from_byte_array", "channel_index", "data"), &VoxelBuffer::set_channel_from_byte_array
	);
	ClassDB::bind_method(
			D_METHOD("debug_print_sdf_y_slices", "scale"), &VoxelBuffer::debug_print_sdf_y_slices, DEFVAL(1.0)
	);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE);
	BIND_ENUM_CONSTANT(CHANNEL_SDF);
	BIND_ENUM_CONSTANT(CHANNEL_COLOR);
	BIND_ENUM_CONSTANT(CHANNEL_INDICES);
	BIND_ENUM_CONSTANT(CHANNEL_WEIGHTS);
	BIND_ENUM_CONSTANT(CHANNEL_DATA5);
	BIND_ENUM_CONSTANT(CHANNEL_DATA6);
	BIND_ENUM_CONSTANT(CHANNEL_DATA7);
	BIND_ENUM_CONSTANT(MAX_CHANNELS);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_SDF_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_COLOR_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_INDICES_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_WEIGHTS_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_DATA5_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_DATA6_BIT);
	BIND_ENUM_CONSTANT(CHANNEL_DATA7_BIT);
	BIND_ENUM_CONSTANT(ALL_CHANNELS_MASK);

	BIND_ENUM_CONSTANT(DEPTH_8_BIT);
	BIND_ENUM_CONSTANT(DEPTH_16_BIT);
	BIND_ENUM_CONSTANT(DEPTH_32_BIT);
	BIND_ENUM_CONSTANT(DEPTH_64_BIT);
	BIND_ENUM_CONSTANT(DEPTH_COUNT);

	BIND_ENUM_CONSTANT(COMPRESSION_NONE);
	BIND_ENUM_CONSTANT(COMPRESSION_UNIFORM);
	BIND_ENUM_CONSTANT(COMPRESSION_COUNT);

	BIND_ENUM_CONSTANT(ALLOCATOR_DEFAULT);
	BIND_ENUM_CONSTANT(ALLOCATOR_POOL);
	BIND_ENUM_CONSTANT(ALLOCATOR_COUNT);

	BIND_CONSTANT(MAX_SIZE);
}

} // namespace zylann::voxel::godot
