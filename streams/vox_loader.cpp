#include "vox_loader.h"
#include "../storage/voxel_buffer.h"
#include <core/os/file_access.h>

namespace vox {

const uint32_t PALETTE_SIZE = 256;

uint32_t g_default_palette[PALETTE_SIZE] = {
	0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff,
	0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
	0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff,
	0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
	0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc,
	0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
	0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc,
	0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
	0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc,
	0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
	0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999,
	0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
	0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099,
	0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
	0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66,
	0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
	0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366,
	0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
	0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33,
	0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
	0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633,
	0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
	0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00,
	0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
	0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600,
	0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
	0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
	0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
	0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700,
	0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
	0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd,
	0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
};

Error load_vox(const String &fpath, Data &data) {
	// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt

	Error err;
	FileAccessRef f = FileAccess::open(fpath, FileAccess::READ, &err);
	if (f == nullptr) {
		return err;
	}

	char magic[5] = { 0 };
	ERR_FAIL_COND_V(f->get_buffer((uint8_t *)magic, 4) != 4, ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(strcmp(magic, "VOX ") != 0, ERR_PARSE_ERROR);

	const uint32_t version = f->get_32();
	ERR_FAIL_COND_V(version != 150, ERR_PARSE_ERROR);

	data.color_indexes.clear();
	data.size = Vector3i();
	data.has_palette = false;
	const size_t flen = f->get_len();

	while (f->get_position() < flen) {
		char chunk_id[5] = { 0 };
		ERR_FAIL_COND_V(f->get_buffer((uint8_t *)chunk_id, 4) != 4, ERR_PARSE_ERROR);

		const uint32_t chunk_size = f->get_32();
		f->get_32(); // child_chunks_size

		if (strcmp(chunk_id, "SIZE") == 0) {
			data.size.x = f->get_32();
			data.size.y = f->get_32();
			data.size.z = f->get_32();
			ERR_FAIL_COND_V(data.size.x > 0xff, ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(data.size.y > 0xff, ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(data.size.z > 0xff, ERR_PARSE_ERROR);

		} else if (strcmp(chunk_id, "XYZI") == 0) {
			data.color_indexes.resize(data.size.x * data.size.y * data.size.z, 0);

			const uint32_t num_voxels = f->get_32();

			for (uint32_t i = 0; i < num_voxels; ++i) {
				const uint32_t z = f->get_8();
				const uint32_t x = f->get_8();
				const uint32_t y = f->get_8();
				const uint32_t c = f->get_8();
				ERR_FAIL_COND_V(x >= static_cast<uint32_t>(data.size.x), ERR_PARSE_ERROR);
				ERR_FAIL_COND_V(y >= static_cast<uint32_t>(data.size.y), ERR_PARSE_ERROR);
				ERR_FAIL_COND_V(z >= static_cast<uint32_t>(data.size.z), ERR_PARSE_ERROR);
				data.color_indexes[Vector3i(x, y, z).get_zxy_index(data.size)] = c;
			}

		} else if (strcmp(chunk_id, "RGBA") == 0) {
			data.palette[0] = Color8{ 0, 0, 0, 0 };
			data.has_palette = true;
			for (uint32_t i = 1; i < data.palette.size(); ++i) {
				Color8 c;
				c.r = f->get_8();
				c.g = f->get_8();
				c.b = f->get_8();
				c.a = f->get_8();
				data.palette[i] = c;
			}
			f->get_32();

		} else {
			// Ignore chunk
			f->seek(f->get_position() + chunk_size);
		}
	}

	return OK;
}

} // namespace vox

Error VoxelVoxLoader::load_from_file(String fpath, Ref<VoxelBuffer> voxels, Ref<VoxelColorPalette> palette) {
	ERR_FAIL_COND_V(voxels.is_null(), ERR_INVALID_PARAMETER);

	const Error err = vox::load_vox(fpath, _data);
	ERR_FAIL_COND_V(err != OK, err);

	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_COLOR;

	const Span<Color8> src_palette =
			_data.has_palette ?
					Span<Color8>(_data.palette) :
					Span<Color8>(reinterpret_cast<Color8 *>(vox::g_default_palette), 0, vox::PALETTE_SIZE);

	const VoxelBuffer::Depth depth = voxels->get_channel_depth(VoxelBuffer::CHANNEL_COLOR);

	Span<uint8_t> dst_raw;
	voxels->create(_data.size);
	voxels->decompress_channel(channel);
	CRASH_COND(!voxels->get_channel_raw(channel, dst_raw));

	if (palette.is_valid() && _data.has_palette) {
		for (size_t i = 0; i < src_palette.size(); ++i) {
			palette->set_color8(i, src_palette[i]);
		}

		switch (depth) {
			case VoxelBuffer::DEPTH_8_BIT: {
				memcpy(dst_raw.data(), _data.color_indexes.data(), _data.color_indexes.size());
			} break;

			case VoxelBuffer::DEPTH_16_BIT: {
				Span<uint16_t> dst = dst_raw.reinterpret_cast_to<uint16_t>();
				for (size_t i = 0; i < dst.size(); ++i) {
					dst[i] = _data.color_indexes[i];
				}
			} break;

			default:
				ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Unsupported depth");
				break;
		}

	} else {
		switch (depth) {
			case VoxelBuffer::DEPTH_8_BIT: {
				for (size_t i = 0; i < dst_raw.size(); ++i) {
					const uint8_t ci = _data.color_indexes[i];
					dst_raw[i] = src_palette[ci].to_u8();
				}
			} break;

			case VoxelBuffer::DEPTH_16_BIT: {
				Span<uint16_t> dst = dst_raw.reinterpret_cast_to<uint16_t>();
				for (size_t i = 0; i < dst.size(); ++i) {
					const uint8_t ci = _data.color_indexes[i];
					dst[i] = src_palette[ci].to_u16();
				}
			} break;

			default:
				ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Unsupported depth");
				break;
		}
	}

	return err;
}

void VoxelVoxLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_file", "fpath", "voxels"), &VoxelVoxLoader::load_from_file);
}
