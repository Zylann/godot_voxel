#include "region_file.h"
#include "../../streams/voxel_block_serializer.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/log.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "../file_utils.h"
#include <algorithm>

namespace zylann::voxel {

namespace {
const uint8_t FORMAT_VERSION = 3;

// Version 2 is like 3, but does not include any format information
const uint8_t FORMAT_VERSION_LEGACY_2 = 2;
// const uint8_t FORMAT_VERSION_LEGACY_1 = 1;

const char *FORMAT_REGION_MAGIC = "VXR_";
const uint32_t MAGIC_AND_VERSION_SIZE = 4 + 1;
const uint32_t FIXED_HEADER_DATA_SIZE = 7 + RegionFormat::CHANNEL_COUNT;
const uint32_t PALETTE_SIZE_IN_BYTES = 256 * 4;
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char *RegionFormat::FILE_EXTENSION = "vxr";

bool RegionFormat::validate() const {
	ERR_FAIL_COND_V(region_size.x < 0 || region_size.x >= static_cast<int>(MAX_BLOCKS_ACROSS), false);
	ERR_FAIL_COND_V(region_size.y < 0 || region_size.y >= static_cast<int>(MAX_BLOCKS_ACROSS), false);
	ERR_FAIL_COND_V(region_size.z < 0 || region_size.z >= static_cast<int>(MAX_BLOCKS_ACROSS), false);
	ERR_FAIL_COND_V(block_size_po2 <= 0, false);

	// Test worst case limits (this does not include arbitrary metadata, so it can't be 100% accurrate...)
	size_t bytes_per_block = 0;
	for (unsigned int i = 0; i < channel_depths.size(); ++i) {
		bytes_per_block += VoxelBufferInternal::get_depth_bit_count(channel_depths[i]) / 8;
	}
	bytes_per_block *= Vector3iUtil::get_volume(Vector3iUtil::create(1 << block_size_po2));
	const size_t sectors_per_block = (bytes_per_block - 1) / sector_size + 1;
	ERR_FAIL_COND_V(sectors_per_block > RegionBlockInfo::MAX_SECTOR_COUNT, false);
	const size_t max_potential_sectors = Vector3iUtil::get_volume(region_size) * sectors_per_block;
	ERR_FAIL_COND_V(max_potential_sectors > RegionBlockInfo::MAX_SECTOR_INDEX, false);

	return true;
}

bool RegionFormat::verify_block(const VoxelBufferInternal &block) const {
	ERR_FAIL_COND_V(block.get_size() != Vector3iUtil::create(1 << block_size_po2), false);
	for (unsigned int i = 0; i < VoxelBufferInternal::MAX_CHANNELS; ++i) {
		ERR_FAIL_COND_V(block.get_channel_depth(i) != channel_depths[i], false);
	}
	return true;
}

static uint32_t get_header_size_v3(const RegionFormat &format) {
	// Which file offset blocks data is starting
	// magic + version + blockinfos
	return MAGIC_AND_VERSION_SIZE + FIXED_HEADER_DATA_SIZE + (format.has_palette ? PALETTE_SIZE_IN_BYTES : 0) +
			Vector3iUtil::get_volume(format.region_size) * sizeof(RegionBlockInfo);
}

static bool save_header(
		FileAccess &f, uint8_t version, const RegionFormat &format, const std::vector<RegionBlockInfo> &block_infos) {
	f.seek(0);

	store_buffer(f, Span<const uint8_t>(reinterpret_cast<const uint8_t *>(FORMAT_REGION_MAGIC), 4));
	f.store_8(version);

	f.store_8(format.block_size_po2);

	f.store_8(format.region_size.x);
	f.store_8(format.region_size.y);
	f.store_8(format.region_size.z);

	for (unsigned int i = 0; i < format.channel_depths.size(); ++i) {
		f.store_8(format.channel_depths[i]);
	}

	f.store_16(format.sector_size);

	if (format.has_palette) {
		f.store_8(0xff);
		for (unsigned int i = 0; i < format.palette.size(); ++i) {
			const Color8 c = format.palette[i];
			f.store_8(c.r);
			f.store_8(c.g);
			f.store_8(c.b);
			f.store_8(c.a);
		}
	} else {
		f.store_8(0x00);
	}

	// TODO Deal with endianess, this should be little-endian
	store_buffer(f,
			Span<const uint8_t>(reinterpret_cast<const uint8_t *>(block_infos.data()),
					block_infos.size() * sizeof(RegionBlockInfo)));

#ifdef DEBUG_ENABLED
	const size_t blocks_begin_offset = f.get_position();
	CRASH_COND(blocks_begin_offset != get_header_size_v3(format));
#endif

	return true;
}

static bool load_header(
		FileAccess &f, uint8_t &out_version, RegionFormat &out_format, std::vector<RegionBlockInfo> &out_block_infos) {
	ERR_FAIL_COND_V(f.get_position() != 0, false);
	ERR_FAIL_COND_V(f.get_length() < MAGIC_AND_VERSION_SIZE, false);

	FixedArray<char, 5> magic;
	fill(magic, '\0');
	ERR_FAIL_COND_V(get_buffer(f, Span<uint8_t>(reinterpret_cast<uint8_t *>(magic.data()), 4)) != 4, false);
	ERR_FAIL_COND_V(strcmp(magic.data(), FORMAT_REGION_MAGIC) != 0, false);

	const uint8_t version = f.get_8();

	if (version == FORMAT_VERSION) {
		out_format.block_size_po2 = f.get_8();

		out_format.region_size.x = f.get_8();
		out_format.region_size.y = f.get_8();
		out_format.region_size.z = f.get_8();

		for (unsigned int i = 0; i < out_format.channel_depths.size(); ++i) {
			const uint8_t d = f.get_8();
			ERR_FAIL_COND_V(d >= VoxelBufferInternal::DEPTH_COUNT, false);
			out_format.channel_depths[i] = static_cast<VoxelBufferInternal::Depth>(d);
		}

		out_format.sector_size = f.get_16();

		const uint8_t palette_size = f.get_8();
		if (palette_size == 0xff) {
			out_format.has_palette = true;
			for (unsigned int i = 0; i < out_format.palette.size(); ++i) {
				Color8 c;
				c.r = f.get_8();
				c.g = f.get_8();
				c.b = f.get_8();
				c.a = f.get_8();
				out_format.palette[i] = c;
			}

		} else if (palette_size == 0x00) {
			out_format.has_palette = false;

		} else {
			ZN_PRINT_ERROR(format("Unexpected palette value: {}", int(palette_size)));
			return false;
		}
	}

	out_version = version;
	out_block_infos.resize(Vector3iUtil::get_volume(out_format.region_size));

	// TODO Deal with endianess
	const size_t blocks_len = out_block_infos.size() * sizeof(RegionBlockInfo);
	const size_t read_size = get_buffer(f, Span<uint8_t>((uint8_t *)out_block_infos.data(), blocks_len));
	ERR_FAIL_COND_V(read_size != blocks_len, false);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RegionFile::RegionFile() {
	// Defaults
	_header.format.block_size_po2 = 4;
	_header.format.region_size = Vector3i(16, 16, 16);
	fill(_header.format.channel_depths, VoxelBufferInternal::DEPTH_8_BIT);
	_header.format.sector_size = 512;
}

RegionFile::~RegionFile() {
	close();
}

Error RegionFile::open(const String &fpath, bool create_if_not_found) {
	close();

	_file_path = fpath;

	Error file_error;
	// Open existing file for read and write permissions. This should not create the file if it doesn't exist.
	// Note, there is no read-only mode supported, because there was no need for it yet.
	Ref<FileAccess> f = open_file(fpath, FileAccess::READ_WRITE, file_error);
	if (file_error != OK) {
		if (create_if_not_found) {
			CRASH_COND(f != nullptr);

			// Checking folders, needed for region "forests"
			const CharString fpath_base_dir = fpath.get_base_dir().utf8();
			const Error dir_err = check_directory_created_using_file_locker(fpath_base_dir.get_data());
			if (dir_err != OK) {
				return ERR_CANT_CREATE;
			}

			// This time, we attempt to create the file
			f = open_file(fpath, FileAccess::WRITE_READ, file_error);
			if (file_error != OK) {
				ERR_PRINT(String("Failed to create file {0}").format(varray(fpath)));
				return file_error;
			}

			_header.version = FORMAT_VERSION;
			ERR_FAIL_COND_V(save_header(**f) == false, ERR_FILE_CANT_WRITE);

		} else {
			return file_error;
		}
	} else {
		CRASH_COND(f.is_null());
		const Error header_error = load_header(**f);
		if (header_error != OK) {
			return header_error;
		}
	}

	_file_access = f;

	// Precalculate location of sectors and which block they contain.
	// This will be useful to know when sectors get moved on insertion and removal

	struct BlockInfoAndIndex {
		RegionBlockInfo b;
		unsigned int i;
	};

	// Filter only present blocks and keep the index around because it represents the 3D position of the block
	std::vector<BlockInfoAndIndex> blocks_sorted_by_offset;
	for (unsigned int i = 0; i < _header.blocks.size(); ++i) {
		const RegionBlockInfo b = _header.blocks[i];
		if (b.data != 0) {
			BlockInfoAndIndex p;
			p.b = b;
			p.i = i;
			blocks_sorted_by_offset.push_back(p);
		}
	}

	std::sort(blocks_sorted_by_offset.begin(), blocks_sorted_by_offset.end(),
			[](const BlockInfoAndIndex &a, const BlockInfoAndIndex &b) {
				return a.b.get_sector_index() < b.b.get_sector_index();
			});

	CRASH_COND(_sectors.size() != 0);
	for (unsigned int i = 0; i < blocks_sorted_by_offset.size(); ++i) {
		const BlockInfoAndIndex b = blocks_sorted_by_offset[i];
		Vector3i bpos = get_block_position_from_index(b.i);
		for (unsigned int j = 0; j < b.b.get_sector_count(); ++j) {
			_sectors.push_back(bpos);
		}
	}

#ifdef DEBUG_ENABLED
	debug_check();
#endif

	return OK;
}

Error RegionFile::close() {
	ZN_PROFILE_SCOPE();
	Error err = OK;
	if (_file_access != nullptr) {
		if (_header_modified) {
			_file_access->seek(MAGIC_AND_VERSION_SIZE);
			if (!save_header(**_file_access)) {
				// TODO Need to do a big pass on these errors codes so we can return meaningful ones...
				// Godot codes are quite limited
				err = ERR_FILE_CANT_WRITE;
			}
		}
		_file_access.unref();
	}
	_sectors.clear();
	return err;
}

bool RegionFile::is_open() const {
	return _file_access != nullptr;
}

bool RegionFile::set_format(const RegionFormat &format) {
	ERR_FAIL_COND_V_MSG(_file_access != nullptr, false, "Can't set format when the file already exists");
	ERR_FAIL_COND_V(!format.validate(), false);

	// This will be the format used to create the next file if not found on open()
	_header.format = format;
	_header.blocks.resize(Vector3iUtil::get_volume(format.region_size));

	return true;
}

const RegionFormat &RegionFile::get_format() const {
	return _header.format;
}

bool RegionFile::is_valid_block_position(const Vector3 position) const {
	return position.x >= 0 && //
			position.y >= 0 && //
			position.z >= 0 && //
			position.x < _header.format.region_size.x && //
			position.y < _header.format.region_size.y && //
			position.z < _header.format.region_size.z;
}

Error RegionFile::load_block(Vector3i position, VoxelBufferInternal &out_block) {
	ERR_FAIL_COND_V(_file_access.is_null(), ERR_FILE_CANT_READ);
	FileAccess &f = **_file_access;

	ERR_FAIL_COND_V(!is_valid_block_position(position), ERR_INVALID_PARAMETER);
	const unsigned int lut_index = get_block_index_in_header(position);
	ERR_FAIL_COND_V(lut_index >= _header.blocks.size(), ERR_INVALID_PARAMETER);
	const RegionBlockInfo &block_info = _header.blocks[lut_index];

	if (block_info.data == 0) {
		return ERR_DOES_NOT_EXIST;
	}

	ERR_FAIL_COND_V(out_block.get_size() != out_block.get_size(), ERR_INVALID_PARAMETER);
	// Configure block format
	for (unsigned int channel_index = 0; channel_index < _header.format.channel_depths.size(); ++channel_index) {
		out_block.set_channel_depth(channel_index, _header.format.channel_depths[channel_index]);
	}

	const unsigned int sector_index = block_info.get_sector_index();
	const unsigned int block_begin = _blocks_begin_offset + sector_index * _header.format.sector_size;

	f.seek(block_begin);

	unsigned int block_data_size = f.get_32();
	CRASH_COND(f.eof_reached());

	ERR_FAIL_COND_V_MSG(!BlockSerializer::decompress_and_deserialize(f, block_data_size, out_block), ERR_PARSE_ERROR,
			String("Failed to read block {0}").format(varray(position)));

	return OK;
}

Error RegionFile::save_block(Vector3i position, VoxelBufferInternal &block) {
	ERR_FAIL_COND_V(_header.format.verify_block(block) == false, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!is_valid_block_position(position), ERR_INVALID_PARAMETER);

	ERR_FAIL_COND_V(_file_access == nullptr, ERR_FILE_CANT_WRITE);
	FileAccess &f = **_file_access;

	// We should be allowed to migrate before write operations
	if (_header.version != FORMAT_VERSION) {
		ERR_FAIL_COND_V(migrate_to_latest(f) == false, ERR_UNAVAILABLE);
	}

	const unsigned int lut_index = get_block_index_in_header(position);
	ERR_FAIL_COND_V(lut_index >= _header.blocks.size(), ERR_INVALID_PARAMETER);
	RegionBlockInfo &block_info = _header.blocks[lut_index];

	if (block_info.data == 0) {
		// The block isn't in the file yet, append at the end

		const unsigned int end_offset = _blocks_begin_offset + _sectors.size() * _header.format.sector_size;
		f.seek(end_offset);
		const unsigned int block_offset = f.get_position();
		// Check position matches the sectors rule
		CRASH_COND((block_offset - _blocks_begin_offset) % _header.format.sector_size != 0);

		BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(block);
		ERR_FAIL_COND_V(!res.success, ERR_INVALID_PARAMETER);
		f.store_32(res.data.size());
		const unsigned int written_size = sizeof(uint32_t) + res.data.size();
		store_buffer(f, to_span(res.data));

		const unsigned int end_pos = f.get_position();
		CRASH_COND_MSG(written_size != (end_pos - block_offset),
				String("written_size: {0}, block_offset: {1}, end_pos: {2}")
						.format(varray(written_size, block_offset, end_pos)));
		pad_to_sector_size(f);

		block_info.set_sector_index((block_offset - _blocks_begin_offset) / _header.format.sector_size);
		block_info.set_sector_count(get_sector_count_from_bytes(written_size));

		for (unsigned int i = 0; i < block_info.get_sector_count(); ++i) {
			_sectors.push_back(position);
		}

		_header_modified = true;

	} else {
		// The block is already in the file

		CRASH_COND(_sectors.size() == 0);

		const int old_sector_index = block_info.get_sector_index();
		const int old_sector_count = block_info.get_sector_count();
		CRASH_COND(old_sector_count < 1);

		BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(block);
		ERR_FAIL_COND_V(!res.success, ERR_INVALID_PARAMETER);
		const std::vector<uint8_t> &data = res.data;
		const size_t written_size = sizeof(uint32_t) + data.size();

		const int new_sector_count = get_sector_count_from_bytes(written_size);
		CRASH_COND(new_sector_count < 1);

		if (new_sector_count <= old_sector_count) {
			// We can write the block at the same spot

			if (new_sector_count < old_sector_count) {
				// The block now uses less sectors, we can compact others.
				remove_sectors_from_block(position, old_sector_count - new_sector_count);
				_header_modified = true;
			}

			const size_t block_offset = _blocks_begin_offset + old_sector_index * _header.format.sector_size;
			f.seek(block_offset);

			f.store_32(data.size());
			store_buffer(f, to_span(data));

			const size_t end_pos = f.get_position();
			CRASH_COND(written_size != (end_pos - block_offset));

		} else {
			// The block now uses more sectors, we have to move others.
			// Note: we could shift blocks forward, but we can also remove the block entirely and rewrite it at the end.
			// Need to investigate if it's worth implementing forward shift instead.
			// TODO Prefer doing an end swap kind of thing?

			// This also shifts the rest of the file so the freed sectors may get re-occupied.
			remove_sectors_from_block(position, old_sector_count);

			const size_t block_offset = _blocks_begin_offset + _sectors.size() * _header.format.sector_size;
			f.seek(block_offset);

			f.store_32(data.size());
			store_buffer(f, to_span(data));

			const size_t end_pos = f.get_position();
			CRASH_COND(written_size != (end_pos - block_offset));

			pad_to_sector_size(f);

			block_info.set_sector_index(_sectors.size());
			for (int i = 0; i < new_sector_count; ++i) {
				_sectors.push_back(Vector3u16(position));
			}

			_header_modified = true;
		}

		block_info.set_sector_count(new_sector_count);
	}

	return OK;
}

void RegionFile::pad_to_sector_size(FileAccess &f) {
	const int64_t rpos = f.get_position() - _blocks_begin_offset;
	if (rpos == 0) {
		return;
	}
	CRASH_COND(rpos < 0);
	const int64_t pad = int64_t(_header.format.sector_size) - (rpos - 1) % int64_t(_header.format.sector_size) - 1;
	CRASH_COND(pad < 0);
	for (int64_t i = 0; i < pad; ++i) {
		// Virtual function called many times, hmmmm...
		f.store_8(0);
	}
}

void RegionFile::remove_sectors_from_block(Vector3i block_pos, unsigned int p_sector_count) {
	ZN_PROFILE_SCOPE();

	// Removes sectors from a block, starting from the last ones.
	// So if a block has 5 sectors and we remove 2, the first 3 will be preserved.
	// Then all following sectors are moved earlier in the file to fill the gap.

	CRASH_COND(_file_access == nullptr);
	CRASH_COND(p_sector_count <= 0);

	FileAccess &f = **_file_access;
	const unsigned int sector_size = _header.format.sector_size;
	const unsigned int old_end_offset = _blocks_begin_offset + _sectors.size() * sector_size;

	const unsigned int block_index = get_block_index_in_header(block_pos);
	CRASH_COND(block_index >= _header.blocks.size());
	RegionBlockInfo &block_info = _header.blocks[block_index];

	unsigned int src_offset =
			_blocks_begin_offset + (block_info.get_sector_index() + block_info.get_sector_count()) * sector_size;

	unsigned int dst_offset = src_offset - p_sector_count * sector_size;

	CRASH_COND(_sectors.size() < p_sector_count);
	CRASH_COND(src_offset - sector_size < dst_offset);
	CRASH_COND(block_info.get_sector_index() + p_sector_count > _sectors.size());
	CRASH_COND(p_sector_count > block_info.get_sector_count());
	CRASH_COND(dst_offset < _blocks_begin_offset);

	std::vector<uint8_t> temp;
	temp.resize(sector_size);

	// TODO There might be a faster way to shrink a file
	// Erase sectors from file
	while (src_offset < old_end_offset) {
		f.seek(src_offset);
		const size_t read_bytes = get_buffer(f, to_span(temp));
		CRASH_COND(read_bytes != sector_size); // Corrupted file

		f.seek(dst_offset);
		store_buffer(f, to_span(temp));

		src_offset += sector_size;
		dst_offset += sector_size;
	}

	// TODO We need to truncate the end of the file since we effectively shortened it,
	// but FileAccess doesn't have any function to do that... so can't rely on EOF either

	// Erase sectors from cache
	_sectors.erase(_sectors.begin() + (block_info.get_sector_index() + block_info.get_sector_count() - p_sector_count),
			_sectors.begin() + (block_info.get_sector_index() + block_info.get_sector_count()));

	const unsigned int old_sector_index = block_info.get_sector_index();

	// Reduce sectors of current block in header.
	if (block_info.get_sector_count() > p_sector_count) {
		block_info.set_sector_count(block_info.get_sector_count() - p_sector_count);
	} else {
		// Block removed
		block_info.data = 0;
	}

	// Shift sector index of following blocks
	if (old_sector_index < _sectors.size()) {
		for (unsigned int i = 0; i < _header.blocks.size(); ++i) {
			RegionBlockInfo &b = _header.blocks[i];
			if (b.data != 0 && b.get_sector_index() > old_sector_index) {
				b.set_sector_index(b.get_sector_index() - p_sector_count);
			}
		}
	}
}

bool RegionFile::save_header(FileAccess &f) {
	// We should be allowed to migrate before write operations.
	if (_header.version != FORMAT_VERSION) {
		ERR_FAIL_COND_V(migrate_to_latest(f) == false, false);
	}
	ERR_FAIL_COND_V(!zylann::voxel::save_header(f, _header.version, _header.format, _header.blocks), false);
	_blocks_begin_offset = f.get_position();
	_header_modified = false;
	return true;
}

bool RegionFile::migrate_from_v2_to_v3(FileAccess &f, RegionFormat &format) {
	ZN_PRINT_VERBOSE(zylann::format("Migrating region file {} from v2 to v3", _file_path));

	// We can migrate if we know in advance what format the file should contain.
	ERR_FAIL_COND_V_MSG(format.block_size_po2 == 0, false, "Cannot migrate without knowing the correct format");

	// Which file offset blocks data is starting
	// magic + version + blockinfos
	const unsigned int old_header_size = Vector3iUtil::get_volume(format.region_size) * sizeof(uint32_t);

	const unsigned int new_header_size = get_header_size_v3(format) - MAGIC_AND_VERSION_SIZE;
	ERR_FAIL_COND_V_MSG(new_header_size < old_header_size, false, "New version is supposed to have larger header");

	const unsigned int extra_bytes_needed = new_header_size - old_header_size;

	f.seek(MAGIC_AND_VERSION_SIZE);
	insert_bytes(f, extra_bytes_needed);

	f.seek(0);

	// Set version because otherwise `save_header` will attempt to migrate again causing stack-overflow
	_header.version = FORMAT_VERSION;

	return save_header(f);
}

bool RegionFile::migrate_to_latest(FileAccess &f) {
	ERR_FAIL_COND_V(_file_path.is_empty(), false);

	uint8_t version = _header.version;

	// Make a backup?
	// {
	// 	DirAccessRef da = DirAccess::create_for_path(_file_path.get_base_dir());
	// 	ERR_FAIL_COND_V_MSG(!da, false, String("Can't make a backup before migrating {0}").format(varray(_file_path)));
	// 	da->copy(_file_path, _file_path + ".backup");
	// }

	if (version == FORMAT_VERSION_LEGACY_2) {
		ERR_FAIL_COND_V(!migrate_from_v2_to_v3(f, _header.format), false);
		version = FORMAT_VERSION;
	}

	if (version != FORMAT_VERSION) {
		ERR_PRINT(String("Invalid file version: {0}").format(varray(version)));
		return false;
	}

	_header.version = version;
	return true;
}

Error RegionFile::load_header(FileAccess &f) {
	ERR_FAIL_COND_V(!zylann::voxel::load_header(f, _header.version, _header.format, _header.blocks), ERR_PARSE_ERROR);
	_blocks_begin_offset = f.get_position();
	return OK;
}

unsigned int RegionFile::get_block_index_in_header(const Vector3i &rpos) const {
	return Vector3iUtil::get_zxy_index(rpos, _header.format.region_size);
}

Vector3i RegionFile::get_block_position_from_index(uint32_t i) const {
	return Vector3iUtil::from_zxy_index(i, _header.format.region_size);
}

uint32_t RegionFile::get_sector_count_from_bytes(uint32_t size_in_bytes) const {
	return (size_in_bytes - 1) / _header.format.sector_size + 1;
}

unsigned int RegionFile::get_header_block_count() const {
	ERR_FAIL_COND_V(!is_open(), 0);
	return _header.blocks.size();
}

bool RegionFile::has_block(Vector3i position) const {
	ERR_FAIL_COND_V(!is_open(), false);
	ERR_FAIL_COND_V(!is_valid_block_position(position), false);
	const unsigned int bi = get_block_index_in_header(position);
	return _header.blocks[bi].data != 0;
}

bool RegionFile::has_block(unsigned int index) const {
	ERR_FAIL_COND_V(!is_open(), false);
	CRASH_COND(index >= _header.blocks.size());
	return _header.blocks[index].data != 0;
}

// Checks to detect some corruption signs in the file
void RegionFile::debug_check() {
	ERR_FAIL_COND(!is_open());
	ERR_FAIL_COND(_file_access == nullptr);
	FileAccess &f = **_file_access;
	const size_t file_len = f.get_length();

	for (unsigned int lut_index = 0; lut_index < _header.blocks.size(); ++lut_index) {
		const RegionBlockInfo &block_info = _header.blocks[lut_index];
		const Vector3i position = get_block_position_from_index(lut_index);
		if (block_info.data == 0) {
			continue;
		}
		const unsigned int sector_index = block_info.get_sector_index();
		const unsigned int block_begin = _blocks_begin_offset + sector_index * _header.format.sector_size;
		if (block_begin >= file_len) {
			ZN_PRINT_ERROR(format("ERROR: LUT {} ({}): offset {} is larger than file size {}", lut_index, position,
					block_begin, file_len));
			continue;
		}
		f.seek(block_begin);
		const size_t block_data_size = f.get_32();
		const size_t pos = f.get_position();
		const size_t remaining_size = file_len - pos;
		if (block_data_size > remaining_size) {
			ZN_PRINT_ERROR(format("ERROR: LUT {} ({}): block size {} at offset {} is larger than remaining size {}",
					lut_index, position, block_data_size, block_begin, remaining_size));
		}
	}
}

} // namespace zylann::voxel
