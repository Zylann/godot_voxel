#include "voxel_stream_region_files.h"
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
const char *REGION_FILE_EXTENSION = "vxr";
} // namespace

VoxelStreamRegionFiles::VoxelStreamRegionFiles() {
	_meta.version = FORMAT_VERSION;
	_meta.block_size_po2 = 4;
	_meta.region_size_po2 = 4;
	_meta.sector_size = 512; // next_power_of_2(_meta.block_size.volume() / 10) // based on compression ratios
	_meta.lod_count = 1;
}

VoxelStreamRegionFiles::~VoxelStreamRegionFiles() {
	close_all_regions();
}

void VoxelStreamRegionFiles::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	BlockRequest r;
	r.voxel_buffer = out_buffer;
	r.origin_in_voxels = origin_in_voxels;
	r.lod = lod;
	Vector<BlockRequest> requests;
	requests.push_back(r);
	emerge_blocks(requests);
}

void VoxelStreamRegionFiles::immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) {
	BlockRequest r;
	r.voxel_buffer = buffer;
	r.origin_in_voxels = origin_in_voxels;
	r.lod = lod;
	Vector<BlockRequest> requests;
	requests.push_back(r);
	immerge_blocks(requests);
}

void VoxelStreamRegionFiles::emerge_blocks(Vector<BlockRequest> &p_blocks) {
	VOXEL_PROFILE_SCOPE(profile_scope);

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

void VoxelStreamRegionFiles::immerge_blocks(Vector<BlockRequest> &p_blocks) {
	VOXEL_PROFILE_SCOPE(profile_scope);

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

VoxelStreamRegionFiles::EmergeResult VoxelStreamRegionFiles::_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {

	VOXEL_PROFILE_SCOPE(profile_scope);
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

	const Vector3i block_size = Vector3i(1 << _meta.block_size_po2);
	const Vector3i region_size = Vector3i(1 << _meta.region_size_po2);

	CRASH_COND(!_meta_loaded);
	ERR_FAIL_COND_V(lod >= _meta.lod_count, EMERGE_FAILED);
	ERR_FAIL_COND_V(block_size != out_buffer->get_size(), EMERGE_FAILED);

	Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	Vector3i region_pos = get_region_position_from_blocks(block_pos);

	CachedRegion *cache = open_region(region_pos, lod, false);
	if (cache == nullptr || !cache->file_exists) {
		return EMERGE_OK_FALLBACK;
	}

	Vector3i block_rpos = block_pos.wrap(region_size);
	int lut_index = get_block_index_in_header(block_rpos);
	const BlockInfo &block_info = cache->header.blocks[lut_index];

	if (block_info.data == 0) {
		return EMERGE_OK_FALLBACK;
	}

	unsigned int sector_index = block_info.get_sector_index();
	//unsigned int sector_count = block_info.get_sector_count();
	int blocks_begin_offset = get_region_header_size();

	FileAccess *f = cache->file_access;

	f->seek(blocks_begin_offset + sector_index * _meta.sector_size);

	unsigned int block_data_size = f->get_32();
	CRASH_COND(f->eof_reached());

	ERR_EXPLAIN(String("Failed to read block {0} at region {1}").format(varray(block_pos.to_vec3(), region_pos.to_vec3())));
	ERR_FAIL_COND_V(!_block_serializer.decompress_and_deserialize(f, block_data_size, **out_buffer), EMERGE_FAILED);

	return EMERGE_OK;
}

void VoxelStreamRegionFiles::pad_to_sector_size(FileAccess *f) {
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

void VoxelStreamRegionFiles::_immerge_block(Ref<VoxelBuffer> voxel_buffer, Vector3i origin_in_voxels, int lod) {

	VOXEL_PROFILE_SCOPE(profile_scope);

	const Vector3i block_size = Vector3i(1 << _meta.block_size_po2);
	const Vector3i region_size = Vector3i(1 << _meta.region_size_po2);

	ERR_FAIL_COND(_directory_path.empty());
	ERR_FAIL_COND(voxel_buffer.is_null());
	ERR_FAIL_COND(voxel_buffer->get_size() != block_size);

	if (!_meta_saved) {
		Error err = save_meta();
		ERR_FAIL_COND(err != OK);
	}

	Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	Vector3i region_pos = get_region_position_from_blocks(block_pos);
	Vector3i block_rpos = block_pos.wrap(region_size);
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

		for (unsigned int i = 0; i < block_info.get_sector_count(); ++i) {
			cache->sectors.push_back(block_rpos);
		}

		cache->header_modified = true;

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
				cache->header_modified = true;
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

			cache->header_modified = true;
		}

		block_info.set_sector_count(new_sector_count);
	}
}

void VoxelStreamRegionFiles::remove_sectors_from_block(CachedRegion *p_region, Vector3i block_rpos, unsigned int p_sector_count) {
	VOXEL_PROFILE_SCOPE(profile_scope);

	// Removes sectors from a block, starting from the last ones.
	// So if a block has 5 sectors and we remove 2, the first 3 will be preserved.
	// Then all following sectors are moved earlier in the file to fill the gap.

	//print_line(String("Removing {0} sectors from region {1}").format(varray(p_sector_count, p_region->position.to_vec3())));

	FileAccess *f = p_region->file_access;
	int blocks_begin_offset = get_region_header_size();
	int old_end_offset = blocks_begin_offset + p_region->sectors.size() * _meta.sector_size;

	unsigned int block_index = get_block_index_in_header(block_rpos);
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

	unsigned int old_sector_index = block_info.get_sector_index();

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
		for (unsigned int i = 0; i < header.blocks.size(); ++i) {
			BlockInfo &b = header.blocks[i];
			if (b.data != 0 && b.get_sector_index() > old_sector_index) {
				b.set_sector_index(b.get_sector_index() - p_sector_count);
			}
		}
	}
}

String VoxelStreamRegionFiles::get_directory() const {
	return _directory_path;
}

void VoxelStreamRegionFiles::set_directory(String dirpath) {
	if (_directory_path != dirpath) {
		_directory_path = dirpath.strip_edges();
		_meta_loaded = false;
		load_meta();
		_change_notify();
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

Error VoxelStreamRegionFiles::save_meta() {

	ERR_FAIL_COND_V(_directory_path == "", ERR_INVALID_PARAMETER);

	Dictionary d;
	d["version"] = _meta.version;
	d["block_size_po2"] = _meta.block_size_po2;
	d["region_size_po2"] = _meta.region_size_po2;
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
	FileAccessRef f = open_file(meta_path, FileAccess::WRITE, &err);
	if (!f) {
		print_error(String("Could not save {0}").format(varray(meta_path)));
		return err;
	}

	f->store_string(json);

	_meta_saved = true;
	_meta_loaded = true;

	return OK;
}

Error VoxelStreamRegionFiles::load_meta() {

	ERR_FAIL_COND_V(_directory_path == "", ERR_INVALID_PARAMETER);

	// Ensure you cleanup previous world before loading another
	CRASH_COND(_region_cache.size() > 0);

	String meta_path = _directory_path.plus_file(META_FILE_NAME);
	String json;

	{
		Error err;
		FileAccessRef f = open_file(meta_path, FileAccess::READ, &err);
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
	ERR_FAIL_COND_V(!u8_from_json_variant(d["block_size_po2"], meta.block_size_po2), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["region_size_po2"], meta.region_size_po2), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["lod_count"], meta.lod_count), ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!s32_from_json_variant(d["sector_size"], meta.sector_size), ERR_PARSE_ERROR);

	ERR_FAIL_COND_V(meta.version < 0, ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(!check_meta(meta), ERR_INVALID_PARAMETER);

	_meta = meta;
	_meta_loaded = true;
	_meta_saved = true;

	return OK;
}

bool VoxelStreamRegionFiles::check_meta(const Meta &meta) {
	ERR_FAIL_COND_V(meta.block_size_po2 < 1 || meta.block_size_po2 > 8, false);
	ERR_FAIL_COND_V(meta.region_size_po2 < 1 || meta.region_size_po2 > 8, false);
	ERR_FAIL_COND_V(meta.lod_count <= 0 || meta.lod_count > 32, false);
	ERR_FAIL_COND_V(meta.sector_size <= 0 || meta.sector_size > 65536, false);
	return true;
}

Vector3i VoxelStreamRegionFiles::get_block_position_from_voxels(const Vector3i &origin_in_voxels) const {
	return origin_in_voxels >> _meta.block_size_po2;
}

Vector3i VoxelStreamRegionFiles::get_region_position_from_blocks(const Vector3i &block_position) const {
	return block_position >> _meta.region_size_po2;
}

void VoxelStreamRegionFiles::close_all_regions() {
	for (unsigned int i = 0; i < _region_cache.size(); ++i) {
		CachedRegion *cache = _region_cache[i];
		close_region(cache);
		memdelete(cache);
	}
	_region_cache.clear();
}

String VoxelStreamRegionFiles::get_region_file_path(const Vector3i &region_pos, unsigned int lod) const {

	Array a;
	a.resize(5);
	a[0] = lod;
	a[1] = region_pos.x;
	a[2] = region_pos.y;
	a[3] = region_pos.z;
	a[4] = REGION_FILE_EXTENSION;
	return _directory_path.plus_file(String("regions/lod{0}/r.{1}.{2}.{3}.{4}").format(a));
}

VoxelStreamRegionFiles::CachedRegion *VoxelStreamRegionFiles::get_region_from_cache(const Vector3i pos, int lod) const {
	// A linear search might be better than a Map data structure,
	// because it's unlikely to have more than about 10 regions cached at a time
	for (unsigned int i = 0; i < _region_cache.size(); ++i) {
		CachedRegion *r = _region_cache[i];
		if (r->position == pos && r->lod == lod) {
			return r;
		}
	}
	return nullptr;
}

VoxelStreamRegionFiles::CachedRegion *VoxelStreamRegionFiles::open_region(const Vector3i region_pos, unsigned int lod, bool create_if_not_found) {

	VOXEL_PROFILE_SCOPE(profile_scope);
	ERR_FAIL_COND_V(!_meta_loaded, nullptr);
	ERR_FAIL_COND_V(lod < 0, nullptr);

	CachedRegion *cache = get_region_from_cache(region_pos, lod);
	if (cache != nullptr) {
		return cache;
	}

	while (_region_cache.size() > _max_open_regions - 1) {
		close_oldest_region();
	}

	const Vector3i region_size = Vector3i(1 << _meta.region_size_po2);

	String fpath = get_region_file_path(region_pos, lod);
	Error existing_file_err;
	FileAccess *existing_f = open_file(fpath, FileAccess::READ_WRITE, &existing_file_err);
	// TODO Cache the fact the file doesnt exist, so we won't need to do a system call to actually check it every time
	// TODO No need to read the header again when it has been read once, we assume no other process will modify region files

	if (existing_f == nullptr || existing_file_err != OK) {
		// Write new file

		if (!create_if_not_found) {
			//print_error(String("Could not open file {0}").format(varray(fpath)));
			return nullptr;
		}

		VOXEL_PROFILE_SCOPE(profile_create_new_file);

		Error dir_err = check_directory_created(fpath.get_base_dir());
		if (dir_err != OK) {
			return nullptr;
		}

		Error file_err;
		FileAccess *f = open_file(fpath, FileAccess::WRITE_READ, &file_err);

		ERR_EXPLAIN("Failed to write file " + fpath + ", error " + String::num_int64(file_err));
		ERR_FAIL_COND_V(!f, nullptr);

		ERR_EXPLAIN("Error " + String::num_int64(file_err));
		ERR_FAIL_COND_V(file_err != OK, nullptr);

		f->store_buffer((uint8_t *)FORMAT_REGION_MAGIC, 4);
		f->store_8(FORMAT_VERSION);

		cache = memnew(CachedRegion);
		cache->file_exists = true;
		cache->file_access = f;
		cache->position = region_pos;
		cache->lod = lod;
		_region_cache.push_back(cache);
		RegionHeader &header = cache->header;

		header.blocks.resize(region_size.volume());

		save_header(cache);

	} else {
		// Read existing
		VOXEL_PROFILE_SCOPE(profile_read_existing);

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

		header.blocks.resize(region_size.volume());

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
	for (unsigned int i = 0; i < header.blocks.size(); ++i) {
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

	for (unsigned int i = 0; i < blocks_sorted_by_offset.size(); ++i) {
		const BlockInfoAndIndex b = blocks_sorted_by_offset[i];
		Vector3i bpos = get_block_position_from_index(b.i);
		for (unsigned int j = 0; j < b.b.get_sector_count(); ++j) {
			cache->sectors.push_back(bpos);
		}
	}

	cache->last_opened = OS::get_singleton()->get_ticks_usec();

	return cache;
}

void VoxelStreamRegionFiles::save_header(CachedRegion *p_region) {

	VOXEL_PROFILE_SCOPE(profile_scope);
	CRASH_COND(p_region->file_access == nullptr);

	RegionHeader &header = p_region->header;
	CRASH_COND(header.blocks.size() == 0);

	// TODO Deal with endianess
	p_region->file_access->store_buffer((const uint8_t *)header.blocks.data(), header.blocks.size() * sizeof(BlockInfo));
	p_region->header_modified = false;
}

void VoxelStreamRegionFiles::close_region(CachedRegion *region) {
	VOXEL_PROFILE_SCOPE(profile_scope);

	if (region->file_access) {
		FileAccess *f = region->file_access;

		// This is really important because the OS can optimize file closing if we didn't write anything
		if (region->header_modified) {
			f->seek(MAGIC_AND_VERSION_SIZE);
			save_header(region);
		}

		memdelete(region->file_access);
		region->file_access = nullptr;
	}
}

void VoxelStreamRegionFiles::close_oldest_region() {
	// Close region assumed to be the least recently used

	if (_region_cache.size() == 0) {
		return;
	}

	int oldest_index = -1;
	uint64_t oldest_time = 0;
	uint64_t now = OS::get_singleton()->get_ticks_usec();

	for (unsigned int i = 0; i < _region_cache.size(); ++i) {
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

unsigned int VoxelStreamRegionFiles::get_block_index_in_header(const Vector3i &rpos) const {
	const Vector3i region_size(1 << _meta.region_size_po2);
	return rpos.get_zxy_index(region_size);
}

Vector3i VoxelStreamRegionFiles::get_block_position_from_index(int i) const {
	const Vector3i region_size(1 << _meta.region_size_po2);
	return Vector3i::from_zxy_index(i, region_size);
}

int VoxelStreamRegionFiles::get_sector_count_from_bytes(int size_in_bytes) const {
	return (size_in_bytes - 1) / _meta.sector_size + 1;
}

int VoxelStreamRegionFiles::get_region_header_size() const {
	// Which file offset blocks data is starting
	// magic + version + blockinfos
	const Vector3i region_size(1 << _meta.region_size_po2);
	return MAGIC_AND_VERSION_SIZE + region_size.volume() * sizeof(BlockInfo);
}

static inline int convert_block_coordinate(int p_x, int old_size, int new_size) {
	return ::udiv(p_x * old_size, new_size);
}

static Vector3i convert_block_coordinates(Vector3i pos, Vector3i old_size, Vector3i new_size) {
	return Vector3i(
			convert_block_coordinate(pos.x, old_size.x, new_size.x),
			convert_block_coordinate(pos.y, old_size.y, new_size.y),
			convert_block_coordinate(pos.z, old_size.z, new_size.z));
}

void VoxelStreamRegionFiles::_convert_files(Meta new_meta) {
	// TODO Converting across different block sizes is untested.
	// I wrote it because it would be too bad to loose large voxel worlds because of a setting change, so one day we may need it

	print_line("Converting region files");
	// This can be a very long and slow operation. Better run this in a thread.

	ERR_FAIL_COND(!_meta_saved);
	ERR_FAIL_COND(!_meta_loaded);

	close_all_regions();

	Ref<VoxelStreamRegionFiles> old_stream;
	old_stream.instance();
	// Keep file cache to a minimum for the old stream, we'll query all blocks once anyways
	old_stream->_max_open_regions = MAX(1, FOPEN_MAX);

	// Backup current folder by renaming it, leaving the current name vacant
	{
		DirAccessRef da = DirAccess::create_for_path(_directory_path);
		ERR_FAIL_COND(!da);
		int i = 0;
		String old_dir;
		while (true) {
			if (i == 0) {
				old_dir = _directory_path + "_old";
			} else {
				old_dir = _directory_path + "_old" + String::num_int64(i);
			}
			if (da->exists(old_dir)) {
				++i;
			} else {
				Error err = da->rename(_directory_path, old_dir);
				ERR_EXPLAIN(String("Failed to rename '{0}' to '{1}', error {2}")
								.format(varray(_directory_path, old_dir, err)));
				ERR_FAIL_COND(err != OK);
				break;
			}
		}

		old_stream->set_directory(old_dir);
		print_line("Data backed up as " + old_dir);
	}

	struct PositionAndLod {
		Vector3i position;
		int lod;
	};

	ERR_FAIL_COND(old_stream->load_meta() != OK);

	std::vector<PositionAndLod> old_region_list;
	Meta old_meta = old_stream->_meta;

	// Get list of all regions from the old stream
	{
		for (int lod = 0; lod < old_meta.lod_count; ++lod) {

			String lod_folder = old_stream->_directory_path.plus_file("regions").plus_file("lod") + String::num_int64(lod);
			String ext = String(".") + REGION_FILE_EXTENSION;

			DirAccessRef da = DirAccess::open(lod_folder);
			if (!da) {
				continue;
			}

			da->list_dir_begin();

			while (true) {
				String fname = da->get_next();
				if (fname == "") {
					break;
				}
				if (da->current_is_dir()) {
					continue;
				}
				if (fname.ends_with(ext)) {
					Vector<String> parts = fname.split(".");
					// r.x.y.z.ext
					ERR_EXPLAIN(String("Found invalid region file: '{0}'").format(varray(fname)));
					ERR_FAIL_COND(parts.size() < 4);
					PositionAndLod p;
					p.position.x = parts[1].to_int();
					p.position.y = parts[2].to_int();
					p.position.z = parts[3].to_int();
					p.lod = lod;
					old_region_list.push_back(p);
				}
			}

			da->list_dir_end();
		}
	}

	_meta = new_meta;
	ERR_FAIL_COND(save_meta() != OK);

	const Vector3i old_block_size = Vector3i(1 << old_meta.block_size_po2);
	const Vector3i new_block_size = Vector3i(1 << _meta.block_size_po2);

	const Vector3i old_region_size = Vector3i(1 << old_meta.region_size_po2);

	// Read all blocks from the old stream and write them into the new one

	for (unsigned int i = 0; i < old_region_list.size(); ++i) {
		PositionAndLod region_info = old_region_list[i];

		const CachedRegion *region = old_stream->open_region(region_info.position, region_info.lod, false);
		if (region == nullptr) {
			continue;
		}

		print_line(String("Converting region lod{0}/{1}").format(varray(region_info.lod, region_info.position.to_vec3())));

		const RegionHeader &header = region->header;
		for (unsigned int j = 0; j < header.blocks.size(); ++j) {
			const BlockInfo bi = header.blocks[j];
			if (bi.data == 0) {
				continue;
			}

			Ref<VoxelBuffer> old_block;
			old_block.instance();
			old_block->create(old_block_size.x, old_block_size.y, old_block_size.z);

			Ref<VoxelBuffer> new_block;
			new_block.instance();
			new_block->create(new_block_size.x, new_block_size.y, new_block_size.z);

			// Load block from old stream
			Vector3i block_rpos = old_stream->get_block_position_from_index(j);
			Vector3i block_pos = block_rpos + region_info.position * old_region_size;
			old_stream->emerge_block(old_block, block_pos * old_block_size << region_info.lod, region_info.lod);

			// Save it in the new one
			if (old_block_size == new_block_size) {
				immerge_block(old_block, block_pos * new_block_size << region_info.lod, region_info.lod);

			} else {
				Vector3i new_block_pos = convert_block_coordinates(block_pos, old_block_size, new_block_size);

				// TODO Support any size? Assuming cubic blocks here
				if (old_block_size.x < new_block_size.x) {

					Vector3i ratio = new_block_size / old_block_size;
					Vector3i rel = block_pos % ratio;

					// Copy to a sub-area of one block
					emerge_block(new_block, new_block_pos * new_block_size << region_info.lod, region_info.lod);

					Vector3i dst_pos = rel * old_block->get_size();

					for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {
						new_block->copy_from(**old_block, Vector3i(), old_block->get_size(), dst_pos, channel_index);
					}

					new_block->compress_uniform_channels();
					immerge_block(new_block, new_block_pos * new_block_size << region_info.lod, region_info.lod);

				} else {

					// Copy to multiple blocks
					Vector3i area = new_block_size / old_block_size;
					Vector3i rpos;

					for (rpos.z = 0; rpos.z < area.z; ++rpos.z) {
						for (rpos.x = 0; rpos.x < area.x; ++rpos.x) {
							for (rpos.y = 0; rpos.y < area.y; ++rpos.y) {

								Vector3i src_min = rpos * new_block->get_size();
								Vector3i src_max = src_min + new_block->get_size();

								for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {
									new_block->copy_from(**old_block, src_min, src_max, Vector3i(), channel_index);
								}

								immerge_block(new_block, (new_block_pos + rpos) * new_block_size << region_info.lod, region_info.lod);
							}
						}
					}
				}
			}
		}
	}

	close_all_regions();

	print_line("Done converting region files");
}

Vector3i VoxelStreamRegionFiles::get_region_size() const {
	return Vector3i(1 << _meta.region_size_po2);
}

Vector3 VoxelStreamRegionFiles::get_region_size_v() const {
	return get_region_size().to_vec3();
}

int VoxelStreamRegionFiles::get_region_size_po2() const {
	return _meta.region_size_po2;
}

int VoxelStreamRegionFiles::get_block_size_po2() const {
	return _meta.block_size_po2;
}

int VoxelStreamRegionFiles::get_lod_count() const {
	return _meta.lod_count;
}

int VoxelStreamRegionFiles::get_sector_size() const {
	return _meta.sector_size;
}

// TODO The following settings are hard to change.
// If files already exist, these settings will be ignored.
// To be applied, files either need to be wiped out or converted, which is a super-heavy operation.
// This can be made easier by adding a button to the inspector to convert existing files just in case

void VoxelStreamRegionFiles::set_region_size_po2(int p_region_size_po2) {
	if (_meta.region_size_po2 == p_region_size_po2) {
		return;
	}
	ERR_EXPLAIN("Can't change existing region size without heavy conversion. Use convert_files().");
	ERR_FAIL_COND(_meta_loaded);
	ERR_FAIL_COND(p_region_size_po2 < 1);
	ERR_FAIL_COND(p_region_size_po2 > 8);
	_meta.region_size_po2 = p_region_size_po2;
	emit_changed();
}

void VoxelStreamRegionFiles::set_block_size_po2(int p_block_size_po2) {
	if (_meta.block_size_po2 == p_block_size_po2) {
		return;
	}
	ERR_EXPLAIN("Can't change existing block size without heavy conversion. Use convert_files().");
	ERR_FAIL_COND(_meta_loaded);
	ERR_FAIL_COND(p_block_size_po2 < 1);
	ERR_FAIL_COND(p_block_size_po2 > 8);
	_meta.block_size_po2 = p_block_size_po2;
	emit_changed();
}

void VoxelStreamRegionFiles::set_sector_size(int p_sector_size) {
	if (_meta.sector_size == p_sector_size) {
		return;
	}
	ERR_EXPLAIN("Can't change existing sector size without heavy conversion. Use convert_files().");
	ERR_FAIL_COND(_meta_loaded);
	ERR_FAIL_COND(p_sector_size < 256);
	ERR_FAIL_COND(p_sector_size > 65536);
	_meta.sector_size = p_sector_size;
	emit_changed();
}

void VoxelStreamRegionFiles::set_lod_count(int p_lod_count) {
	if (_meta.lod_count == p_lod_count) {
		return;
	}
	ERR_EXPLAIN("Can't change existing LOD count without heavy conversion. Use convert_files().");
	ERR_FAIL_COND(_meta_loaded);
	ERR_FAIL_COND(p_lod_count < 1);
	ERR_FAIL_COND(p_lod_count > 32);
	_meta.lod_count = p_lod_count;
	emit_changed();
}

void VoxelStreamRegionFiles::convert_files(Dictionary d) {

	Meta meta;
	meta.version = _meta.version;
	meta.block_size_po2 = int(d["block_size_po2"]);
	meta.region_size_po2 = int(d["region_size_po2"]);
	meta.sector_size = int(d["sector_size"]);
	meta.lod_count = int(d["lod_count"]);

	ERR_EXPLAIN("Invalid setting");
	ERR_FAIL_COND(!check_meta(meta));

	if (!_meta_loaded) {

		if (load_meta() != OK) {
			// New stream, nothing to convert
			_meta = meta;

		} else {
			// Just opened existing stream
			_convert_files(meta);
		}

	} else {
		// That stream was previously used
		_convert_files(meta);
	}

	emit_changed();
}

void VoxelStreamRegionFiles::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_directory", "directory"), &VoxelStreamRegionFiles::set_directory);
	ClassDB::bind_method(D_METHOD("get_directory"), &VoxelStreamRegionFiles::get_directory);

	ClassDB::bind_method(D_METHOD("get_block_size_po2"), &VoxelStreamRegionFiles::get_block_size_po2);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelStreamRegionFiles::get_lod_count);
	ClassDB::bind_method(D_METHOD("get_region_size"), &VoxelStreamRegionFiles::get_region_size_v);
	ClassDB::bind_method(D_METHOD("get_region_size_po2"), &VoxelStreamRegionFiles::get_region_size_po2);
	ClassDB::bind_method(D_METHOD("get_sector_size"), &VoxelStreamRegionFiles::get_sector_size);

	ClassDB::bind_method(D_METHOD("set_block_size_po2"), &VoxelStreamRegionFiles::set_block_size_po2);
	ClassDB::bind_method(D_METHOD("set_lod_count"), &VoxelStreamRegionFiles::set_lod_count);
	ClassDB::bind_method(D_METHOD("set_region_size_po2"), &VoxelStreamRegionFiles::set_region_size_po2);
	ClassDB::bind_method(D_METHOD("set_sector_size"), &VoxelStreamRegionFiles::set_sector_size);

	ClassDB::bind_method(D_METHOD("convert_files", "new_settings"), &VoxelStreamRegionFiles::convert_files);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "directory", PROPERTY_HINT_DIR), "set_directory", "get_directory");

	ADD_GROUP("Dimensions", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "region_size_po2"), "set_region_size_po2", "get_region_size_po2");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "block_size_po2"), "set_block_size_po2", "get_region_size_po2");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sector_size"), "set_sector_size", "get_sector_size");
}
