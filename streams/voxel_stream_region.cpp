#include "voxel_stream_region.h"
#include "../math/rect3i.h"
#include "../util/utility.h"
#include "file_utils.h"
#include <core/io/json.h>
#include <core/os/os.h>
#include <algorithm>

namespace {
const uint8_t FORMAT_VERSION = 1;
const char *FORMAT_REGION_MAGIC = "VXR_";
const char *META_FILE_NAME = "meta.vxrm";
const int MAGIC_AND_VERSION_SIZE = 4 + 1;
} // namespace

VoxelStreamRegion::VoxelStreamRegion() {
	_meta.version = FORMAT_VERSION;
	_meta.block_size = Vector3i(16, 16, 16);
	_meta.region_size = Vector3i(16, 16, 16);
	_meta.sector_size = 512; // next_power_of_2(_meta.block_size.volume() / 10) // based on compression ratios
	_meta.lod_count = 1;
}

VoxelStreamRegion::~VoxelStreamRegion() {
	clear_cache();
}

void VoxelStreamRegion::emerge_blocks(Vector<BlockRequest> &p_blocks) {

	// In order to minimize opening/closing files, requests are grouped according to their region.

	// Had to copy input to sort it, as some areas in the module break if they get responses in different order
	Vector<BlockRequest> sorted_blocks;
	sorted_blocks.append_array(p_blocks);

	SortArray<BlockRequest, BlockRequestComparator> sorter;
	sorter.compare.self = this;
	sorter.sort(sorted_blocks.ptrw(), sorted_blocks.size());

	Vector<BlockRequest> fallback_requests;

	for (int i = 0; i < sorted_blocks.size(); ++i) {
		BlockRequest &r = sorted_blocks.write[i];
		EmergeResult result = _emerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
		if (result == EMERGE_OK_FALLBACK) {
			fallback_requests.push_back(r);
		}
	}

	emerge_blocks_fallback(fallback_requests);
}

void VoxelStreamRegion::immerge_blocks(Vector<BlockRequest> &p_blocks) {

	// Had to copy input to sort it, as some areas in the module break if they get responses in different order
	Vector<BlockRequest> sorted_blocks;
	sorted_blocks.append_array(p_blocks);

	SortArray<BlockRequest, BlockRequestComparator> sorter;
	sorter.compare.self = this;
	sorter.sort(sorted_blocks.ptrw(), sorted_blocks.size());

	for (int i = 0; i < sorted_blocks.size(); ++i) {
		BlockRequest &r = sorted_blocks.write[i];
		_immerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
	}
}

VoxelStreamRegion::EmergeResult VoxelStreamRegion::_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND_V(out_buffer.is_null(), EMERGE_FAILED);

	if (_directory_path.empty()) {
		return EMERGE_OK_FALLBACK;
	}

	if (!_meta_loaded) {
		Error err = load_meta();
		if (err != OK) {
			// Had to add ERR_FILE_CANT_OPEN because that's what Godot actually returns when the file doesn't exist...
			if (!_meta_saved && (err == ERR_FILE_NOT_FOUND || err == ERR_FILE_CANT_OPEN)) {
				// New data folder, save it for first time
				//print_line("Writing meta file");
				Error save_err = save_meta();
				ERR_FAIL_COND_V(save_err != OK, EMERGE_FAILED);
			} else {
				return EMERGE_FAILED;
			}
		}
	}

	CRASH_COND(!_meta_loaded);
	ERR_FAIL_COND_V(lod >= _meta.lod_count, EMERGE_FAILED);
	ERR_FAIL_COND_V(_meta.block_size != out_buffer->get_size(), EMERGE_FAILED);

	Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	Vector3i region_pos = get_region_position_from_blocks(block_pos);

	CachedRegion *cache = open_region(region_pos, lod, false);
	if (cache == nullptr || !cache->file_exists) {
		return EMERGE_OK_FALLBACK;
	}

	Vector3i block_rpos = block_pos.umod(_meta.region_size);
	int lut_index = get_block_index_in_header(block_rpos);
	const BlockInfo &block_info = cache->header.blocks[lut_index];

	if (block_info.data == 0) {
		return EMERGE_OK_FALLBACK;
	}

	int sector_index = block_info.get_sector_index();
	//int sector_count = block_info.get_sector_count();
	int blocks_begin_offset = get_region_header_size();

	FileAccess *f = cache->file_access;

	f->seek(blocks_begin_offset + sector_index * _meta.sector_size);

	int block_data_size = f->get_32();
	CRASH_COND(f->eof_reached());

	ERR_FAIL_COND_V_MSG(!_block_serializer.decompress_and_deserialize(f, block_data_size, **out_buffer), EMERGE_FAILED,
			String("Failed to read block {0} at region {1}").format(varray(block_pos.to_vec3(), region_pos.to_vec3())));

	return EMERGE_OK;
}

void VoxelStreamRegion::pad_to_sector_size(FileAccess *f) {
	int blocks_begin_offset = get_region_header_size();
	int rpos = f->get_position() - blocks_begin_offset;
	if (rpos == 0) {
		return;
	}
	CRASH_COND(rpos < 0);
	int pad = _meta.sector_size - (rpos - 1) % _meta.sector_size - 1;
	for (int i = 0; i < pad; ++i) {
		// TODO Virtual function called many times, hmmmm...
		f->store_8(0);
	}
}

void VoxelStreamRegion::_immerge_block(Ref<VoxelBuffer> voxel_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(_directory_path.empty());
	ERR_FAIL_COND(voxel_buffer.is_null());

	if (!_meta_saved) {
		Error err = save_meta();
		ERR_FAIL_COND(err != OK);
	}

	Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	Vector3i region_pos = get_region_position_from_blocks(block_pos);
	Vector3i block_rpos = block_pos.umod(_meta.region_size);
	//print_line(String("Immerging block {0} r {1}").format(varray(block_pos.to_vec3(), region_pos.to_vec3())));

	CachedRegion *cache = open_region(region_pos, lod, true);
	ERR_FAIL_COND(cache == nullptr);
	FileAccess *f = cache->file_access;

	int lut_index = get_block_index_in_header(block_rpos);
	BlockInfo &block_info = cache->header.blocks[lut_index];
	int blocks_begin_offset = get_region_header_size();

	if (block_info.data == 0) {
		// The block isn't in the file yet, append at the end

		int end_offset = blocks_begin_offset + cache->sectors.size() * _meta.sector_size;
		f->seek(end_offset);
		int block_offset = f->get_position();
		// Check position matches the sectors rule
		CRASH_COND((block_offset - blocks_begin_offset) % _meta.sector_size != 0);

		const std::vector<uint8_t> &data = _block_serializer.serialize_and_compress(**voxel_buffer);
		f->store_32(data.size());
		int written_size = sizeof(int) + data.size();
		f->store_buffer(data.data(), data.size());

		int end_pos = f->get_position();
		CRASH_COND(written_size != (end_pos - block_offset));
		pad_to_sector_size(f);

		block_info.set_sector_index((block_offset - blocks_begin_offset) / _meta.sector_size);
		block_info.set_sector_count(get_sector_count_from_bytes(written_size));

		for (int i = 0; i < block_info.get_sector_count(); ++i) {
			cache->sectors.push_back(block_rpos);
		}

		//print_line(String("Block saved flen={0}").format(varray(f->get_len())));

	} else {
		// The block is already in the file

		CRASH_COND(cache->sectors.size() == 0);

		int old_sector_index = block_info.get_sector_index();
		int old_sector_count = block_info.get_sector_count();
		CRASH_COND(old_sector_count < 1);

		const std::vector<uint8_t> &data = _block_serializer.serialize_and_compress(**voxel_buffer);
		int written_size = sizeof(int) + data.size();

		int new_sector_count = get_sector_count_from_bytes(written_size);
		CRASH_COND(new_sector_count < 1);

		if (new_sector_count <= old_sector_count) {
			// We can write the block at the same spot

			if (new_sector_count < old_sector_count) {
				// The block now uses less sectors, we can compact others.
				remove_sectors_from_block(cache, block_rpos, old_sector_count - new_sector_count);
			}

			int block_offset = blocks_begin_offset + old_sector_index * _meta.sector_size;
			f->seek(block_offset);

			f->store_32(data.size());
			f->store_buffer(data.data(), data.size());

			int end_pos = f->get_position();
			CRASH_COND(written_size != (end_pos - block_offset));

		} else {
			// The block now uses more sectors, we have to move others.
			// Note: we could shift blocks forward, but we can also remove the block entirely and rewrite it at the end.
			// Need to investigate if it's worth implementing forward shift instead.

			remove_sectors_from_block(cache, block_rpos, old_sector_count);

			int block_offset = blocks_begin_offset + cache->sectors.size() * _meta.sector_size;
			f->seek(block_offset);

			f->store_32(data.size());
			f->store_buffer(data.data(), data.size());

			int end_pos = f->get_position();
			CRASH_COND(written_size != (end_pos - block_offset));

			pad_to_sector_size(f);

			block_info.set_sector_index(cache->sectors.size());
			for (int i = 0; i < new_sector_count; ++i) {
				cache->sectors.push_back(block_rpos);
			}
		}

		block_info.set_sector_count(new_sector_count);
	}
}

void VoxelStreamRegion::remove_sectors_from_block(CachedRegion *p_region, Vector3i block_rpos, int p_sector_count) {

	// Removes sectors from a block, starting from the last ones.
	// So if a block has 5 sectors and we remove 2, the first 3 will be preserved.
	// Then all following sectors are moved earlier in the file to fill the gap.

	//print_line(String("Removing {0} sectors from region {1}").format(varray(p_sector_count, p_region->position.to_vec3())));

	FileAccess *f = p_region->file_access;
	int blocks_begin_offset = get_region_header_size();
	int old_end_offset = blocks_begin_offset + p_region->sectors.size() * _meta.sector_size;

	int block_index = get_block_index_in_header(block_rpos);
	BlockInfo &block_info = p_region->header.blocks[block_index];

	int src_offset = blocks_begin_offset + (block_info.get_sector_index() + block_info.get_sector_count()) * _meta.sector_size;
	int dst_offset = src_offset - p_sector_count * _meta.sector_size;

	// Note: removing the last block from a region doesn't make the file invalid, but is not a known use case
	CRASH_COND(p_region->sectors.size() - p_sector_count <= 0);
	CRASH_COND(f == nullptr);
	CRASH_COND(p_sector_count <= 0);
	CRASH_COND(src_offset - _meta.sector_size < dst_offset);
	CRASH_COND(block_info.get_sector_index() + p_sector_count > p_region->sectors.size());
	CRASH_COND(p_sector_count > block_info.get_sector_count());
	CRASH_COND(dst_offset < blocks_begin_offset);

	uint8_t *temp = (uint8_t *)memalloc(_meta.sector_size);

	// TODO There might be a faster way to shrink a file
	// Erase sectors from file
	while (src_offset < old_end_offset) {

		f->seek(src_offset);
		int read_bytes = f->get_buffer(temp, _meta.sector_size);
		CRASH_COND(read_bytes != _meta.sector_size); // Corrupted file

		f->seek(dst_offset);
		f->store_buffer(temp, _meta.sector_size);

		src_offset += _meta.sector_size;
		dst_offset += _meta.sector_size;
	}

	memfree(temp);
	// TODO We need to truncate the end of the file since we effectively shortened it,
	// but FileAccess doesn't have any function to do that... so can't rely on EOF either

	// Erase sectors from cache
	p_region->sectors.erase(
			p_region->sectors.begin() + (block_info.get_sector_index() + block_info.get_sector_count() - p_sector_count),
			p_region->sectors.begin() + (block_info.get_sector_index() + block_info.get_sector_count()));

	int old_sector_index = block_info.get_sector_index();

	// Reduce sectors of current block in header.
	if (block_info.get_sector_count() > p_sector_count) {
		block_info.set_sector_count(block_info.get_sector_count() - p_sector_count);
	} else {
		// Block removed
		block_info.data = 0;
	}

	// Shift sector index of following blocks
	if (old_sector_index < p_region->sectors.size()) {
		RegionHeader &header = p_region->header;
		for (int i = 0; i < header.blocks.size(); ++i) {
			BlockInfo &b = header.blocks[i];
			if (b.data != 0 && b.get_sector_index() > old_sector_index) {
				b.set_sector_index(b.get_sector_index() - p_sector_count);
			}
		}
	}
}

String VoxelStreamRegion::get_directory() const {
	return _directory_path;
}

void VoxelStreamRegion::set_directory(String dirpath) {
	if (_directory_path != dirpath) {
		_directory_path = dirpath.strip_edges();
		_meta_loaded = false;
	}
}

static Array to_varray(const Vector3i &v) {
	Array a;
	a.resize(3);
	a[0] = v.x;
	a[1] = v.y;
	a[2] = v.z;
	return a;
}

static bool u8_from_json_variant(Variant v, uint8_t &i) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT && v.get_type() != Variant::REAL, false);
	int n = v;
	ERR_FAIL_COND_V(n < 0 || n > 255, false);
	i = v;
	return true;
}

static bool s32_from_json_variant(Variant v, int &i) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT && v.get_type() != Variant::REAL, false);
	i = v;
	return true;
}

static bool from_json_varray(Array a, Vector3i &v) {
	ERR_FAIL_COND_V(a.size() != 3, false);
	for (int i = 0; i < 3; ++i) {
		if (!s32_from_json_variant(a[i], v[i])) {
			return false;
		}
	}
	return true;
}

Error VoxelStreamRegion::save_meta() {

	ERR_FAIL_COND_V(_directory_path == "", ERR_INVALID_PARAMETER);

	Dictionary d;
	d["version"] = _meta.version;
	d["block_size"] = to_varray(_meta.block_size);
	d["region_size"] = to_varray(_meta.region_size);
	d["lod_count"] = _meta.lod_count;
	d["sector_size"] = _meta.sector_size;

	String json = JSON::print(d, "\t", true);

	// Make sure the directory exists
	{
		Error err = check_directory_created(_directory_path);
		if (err != OK) {
			ERR_PRINT("Could not save meta");
			return err;
		}
	}

	String meta_path = _directory_path.plus_file(META_FILE_NAME);

	Error err;
	FileAccessRef f = FileAccess::open(meta_path, FileAccess::WRITE, &err);
	++_stats.file_opens;
	if (!f) {
		print_error(String("Could not save {0}").format(varray(meta_path)));
		return err;
	}

	f->store_string(json);

	_meta_saved = true;
	_meta_loaded = true;

	return OK;
}

Error VoxelStreamRegion::load_meta() {

	ERR_FAIL_COND_V(_directory_path == "", ERR_INVALID_PARAMETER);

	// Ensure you cleanup previous world before loading another
	CRASH_COND(_region_cache.size() > 0);

	String meta_path = _directory_path.plus_file(META_FILE_NAME);
	String json;

	{
		Error err;
		FileAccessRef f = FileAccess::open(meta_path, FileAccess::READ, &err);
		++_stats.file_opens;
		if (!f) {
			//print_error(String("Could not load {0}").format(varray(meta_path)));
			return err;
		}
		json = f->get_as_utf8_string();
	}

	// Note: I chose JSON purely for debugging purposes. This file is not meant to be edited by hand.
	// World configuration changes may need a full converter.

	Variant res;
	String json_err_msg;
	int json_err_line;
	Error json_err = JSON::parse(json, res, json_err_msg, json_err_line);
	if (json_err != OK) {
		print_error(String("Error when parsing {0}: line {1}: {2}").format(varray(meta_path, json_err_line, json_err_msg)));
		return json_err;
	}

	Dictionary d = res;
	Meta meta;
	ERR_FAIL_COND_V(!u8_from_json_variant(d["version"], meta.version), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!from_json_varray(d["block_size"], meta.block_size), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!from_json_varray(d["region_size"], meta.region_size), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["lod_count"], meta.lod_count), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!s32_from_json_variant(d["sector_size"], meta.sector_size), ERR_PARSE_ERROR);

	ERR_FAIL_COND_V(meta.version < 0, ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!Rect3i(0, 0, 0, 512, 512, 512).contains(meta.block_size), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!Rect3i(0, 0, 0, 512, 512, 512).contains(meta.region_size), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(meta.lod_count < 0 || meta.lod_count > 32, ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(meta.sector_size < 0 || meta.sector_size > 65536, ERR_PARSE_ERROR);

	_meta = meta;
	_meta_loaded = true;
	_meta_saved = true;

	return OK;
}

Vector3i VoxelStreamRegion::get_block_position_from_voxels(const Vector3i &origin_in_voxels) const {
	return origin_in_voxels.udiv(_meta.block_size);
}

Vector3i VoxelStreamRegion::get_region_position_from_blocks(const Vector3i &block_position) const {
	return block_position.udiv(_meta.region_size);
}

void VoxelStreamRegion::clear_cache() {
	// Clears the cache without saving anything
	for (int i = 0; i < _region_cache.size(); ++i) {
		CachedRegion *cache = _region_cache[i];
		close_region(cache);
		memdelete(cache);
	}
	_region_cache.clear();
}

String VoxelStreamRegion::get_region_file_path(const Vector3i &region_pos, unsigned int lod) const {

	// TODO Separate lods in other region files is a bad idea
	// Better append them to the same regions so we don't re-create more file switching.
	// If a block spans a larger area, it will remain in the region where its origin is.
	Array a;
	a.resize(5);
	a[0] = lod;
	a[1] = region_pos.x;
	a[2] = region_pos.y;
	a[3] = region_pos.z;
	return _directory_path.plus_file(String("regions/lod{0}/r.{1}.{2}.{3}.vxr").format(a));
}

VoxelStreamRegion::CachedRegion *VoxelStreamRegion::get_region_from_cache(const Vector3i pos, int lod) const {
	// A linear search might be better than a Map data structure,
	// because it's unlikely to have more than about 10 regions cached at a time
	for (int i = 0; i < _region_cache.size(); ++i) {
		CachedRegion *r = _region_cache[i];
		if (r->position == pos && r->lod == lod) {
			return r;
		}
	}
	return nullptr;
}

VoxelStreamRegion::CachedRegion *VoxelStreamRegion::open_region(const Vector3i region_pos, unsigned int lod, bool create_if_not_found) {

	ERR_FAIL_COND_V(!_meta_loaded, nullptr);
	ERR_FAIL_COND_V(lod < 0, nullptr);

	CachedRegion *cache = get_region_from_cache(region_pos, lod);
	if (cache != nullptr) {
		return cache;
	}

	while (_region_cache.size() > _max_open_regions - 1) {
		close_oldest_region();
	}

	String fpath = get_region_file_path(region_pos, lod);
	Error existing_file_err;
	FileAccess *existing_f = FileAccess::open(fpath, FileAccess::READ_WRITE, &existing_file_err);
	++_stats.file_opens;
	// TODO Cache the fact the file doesnt exist, so we won't need to do a system call to actually check it every time
	// TODO No need to read the header again when it has been read once, we assume no other process will modify region files

	if (existing_f == nullptr || existing_file_err != OK) {
		// Write new file

		if (!create_if_not_found) {
			//print_error(String("Could not open file {0}").format(varray(fpath)));
			return nullptr;
		}

		Error dir_err = check_directory_created(fpath.get_base_dir());
		if (dir_err != OK) {
			return nullptr;
		}

		Error file_err;
		FileAccess *f = FileAccess::open(fpath, FileAccess::WRITE_READ, &file_err);
		++_stats.file_opens;
		ERR_FAIL_COND_V_MSG(!f, nullptr, "Failed to write file " + fpath + ", error " + String::num_int64(file_err));
		ERR_FAIL_COND_V_MSG(file_err != OK, nullptr, "Error " + String::num_int64(file_err));

		f->store_buffer((uint8_t *)FORMAT_REGION_MAGIC, 4);
		f->store_8(FORMAT_VERSION);

		cache = memnew(CachedRegion);
		cache->file_exists = true;
		cache->file_access = f;
		cache->position = region_pos;
		cache->lod = lod;
		_region_cache.push_back(cache);
		RegionHeader &header = cache->header;

		header.blocks.resize(_meta.region_size.volume());

		save_header(cache);

	} else {
		// Read existing

		uint8_t version;
		if (check_magic_and_version(existing_f, FORMAT_VERSION, FORMAT_REGION_MAGIC, version) != OK) {
			memdelete(existing_f);
			print_error(String("Could not open file {0}, format or version mismatch").format(varray(fpath)));
			return nullptr;
		}

		cache = memnew(CachedRegion);
		cache->file_exists = true;
		cache->file_access = existing_f;
		cache->position = region_pos;
		cache->lod = lod;
		_region_cache.push_back(cache);
		RegionHeader &header = cache->header;

		header.blocks.resize(_meta.region_size.volume());

		// TODO Deal with endianess
		existing_f->get_buffer((uint8_t *)header.blocks.data(), header.blocks.size() * sizeof(BlockInfo));
	}

	// Precalculate location of sectors and which block they contain.
	// This will be useful to know when sectors get moved on insertion and removal

	struct BlockInfoAndIndex {
		BlockInfo b;
		int i;
	};

	// Filter only present blocks and keep the index around because it represents the 3D position of the block
	RegionHeader &header = cache->header;
	std::vector<BlockInfoAndIndex> blocks_sorted_by_offset;
	for (int i = 0; i < header.blocks.size(); ++i) {
		const BlockInfo b = header.blocks[i];
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

	for (int i = 0; i < blocks_sorted_by_offset.size(); ++i) {
		const BlockInfoAndIndex b = blocks_sorted_by_offset[i];
		Vector3i bpos = get_block_position_from_index(b.i);
		for (int j = 0; j < b.b.get_sector_count(); ++j) {
			cache->sectors.push_back(bpos);
		}
	}

	cache->last_opened = OS::get_singleton()->get_ticks_usec();

	return cache;
}

void VoxelStreamRegion::save_header(CachedRegion *p_region) {

	CRASH_COND(p_region->file_access == nullptr);

	RegionHeader &header = p_region->header;
	CRASH_COND(header.blocks.size() == 0);

	// TODO Deal with endianess
	p_region->file_access->store_buffer((const uint8_t *)header.blocks.data(), header.blocks.size() * sizeof(BlockInfo));
}

void VoxelStreamRegion::close_region(CachedRegion *region) {

	if (region->file_access) {
		FileAccess *f = region->file_access;

		// TODO Only save header if it actually changed
		f->seek(MAGIC_AND_VERSION_SIZE);
		save_header(region);

		memdelete(region->file_access);
		region->file_access = nullptr;
	}
}

void VoxelStreamRegion::close_oldest_region() {
	// Close region assumed to be the least recently used

	if (_region_cache.size() == 0) {
		return;
	}

	int oldest_index = -1;
	uint64_t oldest_time = 0;
	uint64_t now = OS::get_singleton()->get_ticks_usec();

	for (int i = 0; i < _region_cache.size(); ++i) {
		CachedRegion *r = _region_cache[i];
		uint64_t time = now - r->last_opened;
		if (time >= oldest_time) {
			oldest_index = i;
		}
	}

	CachedRegion *region = _region_cache[oldest_index];
	_region_cache.erase(_region_cache.begin() + oldest_index);

	close_region(region);
	memdelete(region);
}

int VoxelStreamRegion::get_block_index_in_header(const Vector3i &rpos) const {
	// rpos is the block position relative to the region
	return rpos.y + _meta.region_size.y * (rpos.x + _meta.region_size.x * rpos.z); // ZXY
}

Vector3i VoxelStreamRegion::get_block_position_from_index(int i) const {
	// TODO Move these indexing and de-indexing techniques to a single utility function
	Vector3i pos;
	pos.y = i % _meta.region_size.y;
	pos.x = (i / _meta.region_size.y) % _meta.region_size.x;
	pos.z = i / (_meta.region_size.y * _meta.region_size.x);
	return pos;
}

int VoxelStreamRegion::get_sector_count_from_bytes(int size_in_bytes) const {
	return (size_in_bytes - 1) / _meta.sector_size + 1;
}

int VoxelStreamRegion::get_region_header_size() const {
	// Which file offset blocks data is starting
	// magic + version + blockinfos
	return MAGIC_AND_VERSION_SIZE + _meta.region_size.volume() * sizeof(BlockInfo);
}

void VoxelStreamRegion::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_directory", "directory"), &VoxelStreamRegion::set_directory);
	ClassDB::bind_method(D_METHOD("get_directory"), &VoxelStreamRegion::get_directory);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "directory"), "set_directory", "get_directory");
}
