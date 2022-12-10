#include "compute_shader_resource.h"
#include "../util/godot/curve.h"
#include "../util/godot/image.h"
#include "voxel_engine.h"

namespace zylann::voxel {

ComputeShaderResource::ComputeShaderResource() {}

ComputeShaderResource::~ComputeShaderResource() {
	clear();
}

ComputeShaderResource::ComputeShaderResource(ComputeShaderResource &&other) {
	_rid = other._rid;
	_type = other._type;
	other._rid = RID();
}

void ComputeShaderResource::clear() {
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

void ComputeShaderResource::create_texture(Image &image) {
	clear();

	RenderingDevice::DataFormat data_format;
	ERR_FAIL_COND_MSG(!image_to_normalized_rd_format(image.get_format(), data_format), "Image format not handled");

	_type = TYPE_TEXTURE;
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
	ERR_FAIL_COND_MSG(_rid.is_null(), "Failed to create texture");
}

void ComputeShaderResource::create_texture(Curve &curve) {
	const unsigned int width = curve.get_bake_resolution();

	PackedByteArray data;
	data.resize(width * sizeof(float));

	{
		uint8_t *wd8 = data.ptrw();
		float *wd = (float *)wd8;

		for (unsigned int i = 0; i < width; ++i) {
			const float t = i / static_cast<float>(width);
			wd[i] = curve.sample_baked(t);
			print_line(String("X: {0}, Y: {1}").format(varray(t, wd[i])));
		}
	}

	Ref<Image> image = Image::create_from_data(width, 1, false, Image::FORMAT_RF, data);
	create_texture(**image);
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

} // namespace zylann::voxel
