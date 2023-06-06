#include "vox_loader.h"
#include "../../meshers/cubes/voxel_color_palette.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/dstack.h"
#include "vox_data.h"

namespace zylann::voxel {

int /*Error*/ VoxelVoxLoader::load_from_file(String fpath, Ref<gd::VoxelBuffer> p_voxels,
		Ref<VoxelColorPalette> palette, gd::VoxelBuffer::ChannelId dst_channel) {
	ZN_DSTACK();
	ERR_FAIL_INDEX_V(dst_channel, gd::VoxelBuffer::MAX_CHANNELS, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_voxels.is_null(), ERR_INVALID_PARAMETER);
	VoxelBufferInternal &voxels = p_voxels->get_buffer();

	zylann::voxel::magica::Data data;
	Error load_err = data.load_from_file(fpath);
	ERR_FAIL_COND_V(load_err != OK, load_err);

	const zylann::voxel::magica::Model &model = data.get_model(0);

	Span<const Color8> src_palette = to_span_const(data.get_palette());
	const VoxelBufferInternal::Depth depth = voxels.get_channel_depth(VoxelBufferInternal::CHANNEL_COLOR);

	Span<uint8_t> dst_raw;
	voxels.create(model.size);
	voxels.decompress_channel(dst_channel);
	CRASH_COND(!voxels.get_channel_raw(dst_channel, dst_raw));

	if (palette.is_valid()) {
		for (size_t i = 0; i < src_palette.size(); ++i) {
			palette->set_color8(i, src_palette[i]);
		}

		switch (depth) {
			case VoxelBufferInternal::DEPTH_8_BIT: {
				memcpy(dst_raw.data(), model.color_indexes.data(), model.color_indexes.size());
			} break;

			case VoxelBufferInternal::DEPTH_16_BIT: {
				Span<uint16_t> dst = dst_raw.reinterpret_cast_to<uint16_t>();
				for (size_t i = 0; i < dst.size(); ++i) {
					dst[i] = model.color_indexes[i];
				}
			} break;

			default:
				ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Unsupported depth");
				break;
		}

	} else {
		switch (depth) {
			case VoxelBufferInternal::DEPTH_8_BIT: {
				for (size_t i = 0; i < dst_raw.size(); ++i) {
					const uint8_t ci = model.color_indexes[i];
					dst_raw[i] = src_palette[ci].to_u8();
				}
			} break;

			case VoxelBufferInternal::DEPTH_16_BIT: {
				Span<uint16_t> dst = dst_raw.reinterpret_cast_to<uint16_t>();
				for (size_t i = 0; i < dst.size(); ++i) {
					const uint8_t ci = model.color_indexes[i];
					dst[i] = src_palette[ci].to_u16();
				}
			} break;

			default:
				ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Unsupported depth");
				break;
		}
	}

	return load_err;
}

void VoxelVoxLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_file", "fpath", "voxels", "palette", "dst_channel"),
			&VoxelVoxLoader::load_from_file, DEFVAL(gd::VoxelBuffer::CHANNEL_COLOR));
}

} // namespace zylann::voxel
