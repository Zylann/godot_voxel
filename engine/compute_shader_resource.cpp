#include "compute_shader_resource.h"
#include "../util/dstack.h"
#include "../util/godot/classes/curve.h"
#include "../util/godot/classes/image.h"
#include "../util/godot/classes/rd_texture_format.h"
#include "../util/godot/classes/rd_texture_view.h"
#include "../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../util/profiling.h"
#include "voxel_engine.h"

namespace zylann::voxel {

ComputeShaderResource::ComputeShaderResource() {}

ComputeShaderResource::~ComputeShaderResource() {
	ZN_DSTACK();
	clear();
}

ComputeShaderResource::ComputeShaderResource(ComputeShaderResource &&other) {
	_rid = other._rid;
	_type = other._type;
	other._rid = RID();
}

void ComputeShaderResource::clear() {
	ZN_DSTACK();
	if (_rid.is_valid()) {
		RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
		free_rendering_device_rid(rd, _rid);
		_rid = RID();
	}
}

bool ComputeShaderResource::is_valid() const {
	return _rid.is_valid();
}

ComputeShaderResource::Type ComputeShaderResource::get_type() const {
	return _type;
}

bool image_to_normalized_rd_format(Image::Format image_format, RenderingDevice::DataFormat &out_rd_format) {
	// TODO Setup swizzles for unused components
	// but for now the module's internal shaders don't rely on that

	switch (image_format) {
		// 8-bit formats
		case Image::FORMAT_L8:
		case Image::FORMAT_R8:
			out_rd_format = RenderingDevice::DATA_FORMAT_R8_UNORM;
			break;
		case Image::FORMAT_RG8:
			out_rd_format = RenderingDevice::DATA_FORMAT_R8G8_UNORM;
			break;
		case Image::FORMAT_RGB8:
			out_rd_format = RenderingDevice::DATA_FORMAT_R8G8B8_UNORM;
			break;
		case Image::FORMAT_RGBA8:
			out_rd_format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
			break;
		// 16-bit float formats
		case Image::FORMAT_RH:
			out_rd_format = RenderingDevice::DATA_FORMAT_R16_SFLOAT;
			break;
		case Image::FORMAT_RGH:
			out_rd_format = RenderingDevice::DATA_FORMAT_R16G16_SFLOAT;
			break;
		case Image::FORMAT_RGBH:
			out_rd_format = RenderingDevice::DATA_FORMAT_R16G16B16_SFLOAT;
			break;
		case Image::FORMAT_RGBAH:
			out_rd_format = RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT;
			break;
		// 32-bit float formats
		case Image::FORMAT_RF:
			out_rd_format = RenderingDevice::DATA_FORMAT_R32_SFLOAT;
			break;
		case Image::FORMAT_RGF:
			out_rd_format = RenderingDevice::DATA_FORMAT_R32G32_SFLOAT;
			break;
		case Image::FORMAT_RGBF:
			out_rd_format = RenderingDevice::DATA_FORMAT_R32G32B32_SFLOAT;
			break;
		case Image::FORMAT_RGBAF:
			out_rd_format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
			break;
		default:
			return false;
			// More formats may be added if we need them
	}
	return true;
}

void ComputeShaderResource::create_texture_2d(const Image &image) {
	ZN_PROFILE_SCOPE();
	clear();

	RenderingDevice::DataFormat data_format;
	ERR_FAIL_COND_MSG(!image_to_normalized_rd_format(image.get_format(), data_format), "Image format not handled");

	_type = TYPE_TEXTURE_2D;
	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();

	// Size can vary each time so we have to recreate the format...
	Ref<RDTextureFormat> texture_format;
	texture_format.instantiate();
	texture_format->set_width(image.get_width());
	texture_format->set_height(image.get_height());
	texture_format->set_format(data_format);
	texture_format->set_usage_bits(RenderingDevice::TEXTURE_USAGE_STORAGE_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
			RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT);
	texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
	// TODO Do I need multisample if I want filtering?

	Ref<RDTextureView> texture_view;
	texture_view.instantiate();

	TypedArray<PackedByteArray> data_array;
	data_array.append(image.get_data());

	_rid = texture_create(rd, **texture_format, **texture_view, data_array);
	// RID::is_null() is not available in GDExtension
	ERR_FAIL_COND_MSG(!_rid.is_valid(), "Failed to create texture");
}

void ComputeShaderResource::create_texture_2d(const Curve &curve) {
	ZN_PROFILE_SCOPE();
	const unsigned int width = curve.get_bake_resolution();

	PackedByteArray data;
	data.resize(width * sizeof(float));

	{
		uint8_t *wd8 = data.ptrw();
		float *wd = (float *)wd8;

		for (unsigned int i = 0; i < width; ++i) {
			const float t = i / static_cast<float>(width);
			// TODO Thread-safety: `sample_baked` can actually be a WRITING method! The baked cache is lazily created
			wd[i] = curve.sample_baked(t);
			// print_line(String("X: {0}, Y: {1}").format(varray(t, wd[i])));
		}
	}

	Ref<Image> image = Image::create_from_data(width, 1, false, Image::FORMAT_RF, data);
	create_texture_2d(**image);
}

template <typename T>
void zxy_grid_to_zyx(Span<const T> src, Span<T> dst, Vector3i size) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(Vector3iUtil::is_valid_size(size));
	ZN_ASSERT(Vector3iUtil::get_volume(size) == int64_t(src.size()));
	ZN_ASSERT(src.size() == dst.size());
	Vector3i pos;
	for (pos.z = 0; pos.z < size.z; ++pos.z) {
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = 0; pos.y < size.y; ++pos.y) {
				const unsigned int src_i = Vector3iUtil::get_zxy_index(pos, size);
				const unsigned int dst_i = Vector3iUtil::get_zyx_index(pos, size);
				dst[dst_i] = src[src_i];
			}
		}
	}
}

void ComputeShaderResource::create_texture_3d_zxy(Span<const float> fdata_zxy, Vector3i size) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT(Vector3iUtil::is_valid_size(size));
	ZN_ASSERT(Vector3iUtil::get_volume(size) == int64_t(fdata_zxy.size()));

	clear();

	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();

	Ref<RDTextureFormat> texture_format;
	texture_format.instantiate();
	texture_format->set_width(size.x);
	texture_format->set_height(size.y);
	texture_format->set_depth(size.z);
	texture_format->set_format(RenderingDevice::DATA_FORMAT_R32_SFLOAT);
	texture_format->set_usage_bits(RenderingDevice::TEXTURE_USAGE_STORAGE_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
			RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT);
	texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_3D);

	Ref<RDTextureView> texture_view;
	texture_view.instantiate();

	TypedArray<PackedByteArray> data_array;
	{
		PackedByteArray pba;
		pba.resize(sizeof(float) * Vector3iUtil::get_volume(size));
		uint8_t *pba_w = pba.ptrw();
		zxy_grid_to_zyx(fdata_zxy, Span<float>(reinterpret_cast<float *>(pba_w), pba.size()), size);
		data_array.append(pba);
	}

	_rid = texture_create(rd, **texture_format, **texture_view, data_array);
	ERR_FAIL_COND_MSG(!_rid.is_valid(), "Failed to create texture");

	_type = TYPE_TEXTURE_3D;
}

void ComputeShaderResource::create_storage_buffer(const PackedByteArray &data) {
	clear();
	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
	_rid = rd.storage_buffer_create(data.size(), data);
	ERR_FAIL_COND(!_rid.is_valid());
	_type = TYPE_STORAGE_BUFFER;
}

void ComputeShaderResource::update_storage_buffer(const PackedByteArray &data) {
	ERR_FAIL_COND(!_rid.is_valid());
	ERR_FAIL_COND(_type != TYPE_STORAGE_BUFFER);
	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
	const Error err = zylann::update_storage_buffer(rd, _rid, 0, data.size(), data);
	ERR_FAIL_COND_MSG(err != OK, String("Failed to update storage buffer (error {0})").format(varray(err)));
}

RID ComputeShaderResource::get_rid() const {
	return _rid;
}

void ComputeShaderResource::operator=(ComputeShaderResource &&src) {
	clear();
	_rid = src._rid;
	_type = src._type;
	src._rid = RID();
}

void transform3d_to_mat4(const Transform3D &t, Span<float> dst) {
	// Based on `material_storage.h`

	dst[0] = t.basis.rows[0].x;
	dst[1] = t.basis.rows[1].x;
	dst[2] = t.basis.rows[2].x;
	dst[3] = 0.f;
	dst[4] = t.basis.rows[0].y;
	dst[5] = t.basis.rows[1].y;
	dst[6] = t.basis.rows[2].y;
	dst[7] = 0.f;
	dst[8] = t.basis.rows[0].z;
	dst[9] = t.basis.rows[1].z;
	dst[10] = t.basis.rows[2].z;
	dst[11] = 0.f;
	dst[12] = t.origin.x;
	dst[13] = t.origin.y;
	dst[14] = t.origin.z;
	dst[15] = 1.f;

	// This was based on DirectMultiMeshInstance BUT NO, we have to do the transposed way, because... who knows.

	// dst[0] = t.basis.rows[0].x;
	// dst[1] = t.basis.rows[1].x;
	// dst[2] = t.basis.rows[2].x;
	// dst[3] = t.origin.x;

	// dst[4] = t.basis.rows[0].y;
	// dst[5] = t.basis.rows[1].y;
	// dst[6] = t.basis.rows[2].y;
	// dst[7] = t.origin.y;

	// dst[8] = t.basis.rows[0].z;
	// dst[9] = t.basis.rows[1].z;
	// dst[10] = t.basis.rows[2].z;
	// dst[11] = t.origin.z;

	// dst[12] = 0.f;
	// dst[13] = 0.f;
	// dst[14] = 0.f;
	// dst[15] = 1.f;
}

} // namespace zylann::voxel
