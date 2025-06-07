#include "compute_shader_resource.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/curve.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/rd_texture_format.h"
#include "../../util/godot/classes/rd_texture_view.h"
#include "../../util/godot/classes/rendering_device.h"
#include "../../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "../voxel_engine.h"

namespace zylann::voxel {

// ComputeShaderResourceInternal::ComputeShaderResourceInternal() {}

// ComputeShaderResourceInternal::~ComputeShaderResourceInternal() {
// 	ZN_DSTACK();
// 	clear();
// }

// ComputeShaderResource::ComputeShaderResource(ComputeShaderResource &&other) {
// 	_rid = other._rid;
// 	_type = other._type;
// 	other._rid = RID();
// }

void ComputeShaderResourceInternal::clear(RenderingDevice &rd) {
	ZN_DSTACK();
	if (rid.is_valid()) {
		ZN_PRINT_VERBOSE(format("Freeing VoxelRD resource type {}", type));
		zylann::godot::free_rendering_device_rid(rd, rid);
		rid = RID();
	}
}

bool ComputeShaderResourceInternal::is_valid() const {
	return rid.is_valid();
}

// ComputeShaderResourceInternal::Type ComputeShaderResourceInternal::get_type() const {
// 	return _type;
// }

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

void ComputeShaderResourceInternal::create_texture_2d(RenderingDevice &rd, const Image &image) {
	ZN_PROFILE_SCOPE();
	ZN_PRINT_VERBOSE(format(
			"Creating VoxelRD texture2d {}x{} format {}", image.get_width(), image.get_height(), image.get_format()
	));
	clear(rd);

	RenderingDevice::DataFormat data_format;
	ERR_FAIL_COND_MSG(!image_to_normalized_rd_format(image.get_format(), data_format), "Image format not handled");

	type = TYPE_TEXTURE_2D;

	// Size can vary each time so we have to recreate the format...
	Ref<RDTextureFormat> texture_format;
	texture_format.instantiate();
	texture_format->set_width(image.get_width());
	texture_format->set_height(image.get_height());
	texture_format->set_format(data_format);
	texture_format->set_usage_bits(
			RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT
	);
	texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
	// TODO Do I need multisample if I want filtering?

	Ref<RDTextureView> texture_view;
	texture_view.instantiate();

	TypedArray<PackedByteArray> data_array;
	data_array.append(image.get_data());

	rid = zylann::godot::texture_create(rd, **texture_format, **texture_view, data_array);
	// RID::is_null() is not available in GDExtension
	ERR_FAIL_COND_MSG(!rid.is_valid(), "Failed to create texture");
}

void ComputeShaderResourceInternal::create_texture_2d(RenderingDevice &rd, const Curve &curve) {
	ZN_PROFILE_SCOPE();
	const Image::Format image_format = Image::FORMAT_RF;
	const unsigned int width = curve.get_bake_resolution();

	ZN_PRINT_VERBOSE(format("Creating VoxelRD curve texture {}x1 format {}", width, image_format));

	PackedByteArray data;
	data.resize(width * sizeof(float));

	const math::Interval curve_domain = zylann::godot::get_curve_domain(curve);
	const float curve_domain_range = curve_domain.length();

	{
		uint8_t *wd8 = data.ptrw();
		float *wd = (float *)wd8;

		for (unsigned int i = 0; i < width; ++i) {
			const float t = curve_domain.min + curve_domain_range * (i / static_cast<float>(width));
			// TODO Thread-safety: `sample_baked` can actually be a WRITING method! The baked cache is lazily created
			wd[i] = curve.sample_baked(t);
			// print_line(String("X: {0}, Y: {1}").format(varray(t, wd[i])));
		}
	}

	Ref<Image> image = Image::create_from_data(width, 1, false, image_format, data);
	create_texture_2d(rd, **image);
}

template <typename T>
void zxy_grid_to_zyx(Span<const T> src, Span<T> dst, Vector3i size) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(Vector3iUtil::is_valid_size(size));
	ZN_ASSERT(Vector3iUtil::get_volume_u64(size) == src.size());
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

void ComputeShaderResourceInternal::create_texture_3d_float32(
		RenderingDevice &rd,
		const PackedByteArray &data,
		const Vector3i size
) {
	ZN_PROFILE_SCOPE();
	ZN_PRINT_VERBOSE(format("Creating VoxelRD texture3d {}x{}x{} float32", size.x, size.y, size.z));

	ZN_ASSERT(Vector3iUtil::is_valid_size(size));

	const size_t expected_size_in_bytes = Vector3iUtil::get_volume_u64(size) * sizeof(float);
	ZN_ASSERT(expected_size_in_bytes == static_cast<size_t>(data.size()));

	clear(rd);

	Ref<RDTextureFormat> texture_format;
	texture_format.instantiate();
	texture_format->set_width(size.x);
	texture_format->set_height(size.y);
	texture_format->set_depth(size.z);
	texture_format->set_format(RenderingDevice::DATA_FORMAT_R32_SFLOAT);
	texture_format->set_usage_bits(
			RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT
	);
	texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_3D);

	Ref<RDTextureView> texture_view;
	texture_view.instantiate();

	TypedArray<PackedByteArray> data_array;
	data_array.append(data);

	rid = zylann::godot::texture_create(rd, **texture_format, **texture_view, data_array);
	ERR_FAIL_COND_MSG(!rid.is_valid(), "Failed to create texture");

	type = TYPE_TEXTURE_3D;
}

void ComputeShaderResourceInternal::create_storage_buffer(RenderingDevice &rd, const PackedByteArray &data) {
	clear(rd);
	ZN_PRINT_VERBOSE(format("Creating VoxelRD storage buffer {}b", data.size()));
	rid = rd.storage_buffer_create(data.size(), data);
	ERR_FAIL_COND(!rid.is_valid());
	type = TYPE_STORAGE_BUFFER;
}

void ComputeShaderResourceInternal::update_storage_buffer(RenderingDevice &rd, const PackedByteArray &data) {
	ERR_FAIL_COND(!rid.is_valid());
	ERR_FAIL_COND(type != TYPE_STORAGE_BUFFER);
	const Error err = zylann::godot::update_storage_buffer(rd, rid, 0, data.size(), data);
	ERR_FAIL_COND_MSG(err != OK, String("Failed to update storage buffer (error {0})").format(varray(err)));
}

// RID ComputeShaderResourceInternal::get_rid() const {
// 	return _rid;
// }

// void ComputeShaderResourceInternal::operator=(ComputeShaderResourceInternal &&src) {
// 	clear();
// 	_rid = src._rid;
// 	_type = src._type;
// 	src._rid = RID();
// }

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ComputeShaderResource> ComputeShaderResourceFactory::create_texture_2d(const Ref<Image> &image) {
	ZN_ASSERT(image.is_valid());

	std::shared_ptr<ComputeShaderResource> res = make_shared_instance<ComputeShaderResource>();
	res->_type = ComputeShaderResourceInternal::TYPE_TEXTURE_2D;

	VoxelEngine::get_singleton().push_gpu_task_f([res, image](GPUTaskContext &ctx) {
		res->_internal.create_texture_2d(ctx.rendering_device, **image);
	});
	return res;
}

std::shared_ptr<ComputeShaderResource> ComputeShaderResourceFactory::create_texture_2d(const Ref<Curve> &curve) {
	ZN_ASSERT(curve.is_valid());

	std::shared_ptr<ComputeShaderResource> res = make_shared_instance<ComputeShaderResource>();
	res->_type = ComputeShaderResourceInternal::TYPE_TEXTURE_2D;

	VoxelEngine::get_singleton().push_gpu_task_f([res, curve](GPUTaskContext &ctx) {
		res->_internal.create_texture_2d(ctx.rendering_device, **curve);
	});
	return res;
}

std::shared_ptr<ComputeShaderResource> ComputeShaderResourceFactory::create_texture_3d_zxy(
		Span<const float> fdata_zxy,
		const Vector3i size
) {
	ZN_ASSERT(Vector3iUtil::is_valid_size(size));
	// Note, this array is refcounted so we can pass it to the async queue. It is also what the RD expects so we
	// minimize allocations for intermediate objects
	PackedByteArray pba;
	pba.resize(sizeof(float) * Vector3iUtil::get_volume_u64(size));
	uint8_t *pba_w = pba.ptrw();
	zxy_grid_to_zyx(fdata_zxy, Span<float>(reinterpret_cast<float *>(pba_w), pba.size()), size);

	std::shared_ptr<ComputeShaderResource> res = make_shared_instance<ComputeShaderResource>();
	res->_type = ComputeShaderResourceInternal::TYPE_TEXTURE_3D;

	VoxelEngine::get_singleton().push_gpu_task_f([res, pba, size](GPUTaskContext &ctx) {
		res->_internal.create_texture_3d_float32(ctx.rendering_device, pba, size);
	});
	return res;
}

std::shared_ptr<ComputeShaderResource> ComputeShaderResourceFactory::create_storage_buffer(const PackedByteArray &data
) {
	std::shared_ptr<ComputeShaderResource> res = make_shared_instance<ComputeShaderResource>();
	res->_type = ComputeShaderResourceInternal::TYPE_STORAGE_BUFFER;

	VoxelEngine::get_singleton().push_gpu_task_f([res, data](GPUTaskContext &ctx) {
		res->_internal.create_storage_buffer(ctx.rendering_device, data);
	});
	return res;
}

ComputeShaderResource::~ComputeShaderResource() {
	RID rid = _internal.rid;

	// _internal = ComputeShaderResourceInternal();
	// _internal.type = ComputeShaderResourceInternal::TYPE_DEINITIALIZED;

	VoxelEngine::get_singleton().push_gpu_task_f([rid](GPUTaskContext &ctx) {
		zylann::godot::free_rendering_device_rid(ctx.rendering_device, rid);
	});
}

void ComputeShaderResource::update_storage_buffer(
		const std::shared_ptr<ComputeShaderResource> &res,
		const PackedByteArray &data
) {
	VoxelEngine::get_singleton().push_gpu_task_f([res, data](GPUTaskContext &ctx) {
		res->_internal.create_storage_buffer(ctx.rendering_device, data);
	});
}

RID ComputeShaderResource::get_rid() const {
	// TODO Assert that we are on the GPU tasks thread
	return _internal.rid;
}

ComputeShaderResourceInternal::Type ComputeShaderResource::get_type() const {
	return _type;
}

} // namespace zylann::voxel
