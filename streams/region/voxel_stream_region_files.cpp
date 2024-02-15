#include "voxel_stream_region_files.h"
#include "../../engine/voxel_engine.h"
#include "../../util/godot/classes/directory.h"
#include "../../util/godot/classes/json.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/io/log.h"
#include "../../util/math/box3i.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"

#include <algorithm>

namespace zylann::voxel {

namespace {
const uint8_t FORMAT_VERSION = 3;

// Version 2 is the same as version 3, except region files use version 3 of their specification.
const uint8_t FORMAT_VERSION_LEGACY_2 = 2;

const uint8_t FORMAT_VERSION_LEGACY_1 = 1;
const char *META_FILE_NAME = "meta.vxrm";

} // namespace

// Sorts a sequence without modifying it, returning a sorted list of pointers
template <typename T, typename Comparer_T>
void get_sorted_indices(Span<T> sequence, Comparer_T comparer, std::vector<unsigned int> &out_sorted_indices) {
	struct Compare {
		Span<T> sequence;
		Comparer_T comparer;
		inline bool operator()(unsigned int ia, unsigned int ib) const {
			return comparer(sequence[ia], sequence[ib]);
		}
	};
	out_sorted_indices.resize(sequence.size());
	for (unsigned int i = 0; i < sequence.size(); ++i) {
		out_sorted_indices[i] = i;
	}
	SortArray<unsigned int, Compare> sort_array;
	sort_array.compare.sequence = sequence;
	sort_array.compare.comparer = comparer;
	sort_array.sort(out_sorted_indices.data(), out_sorted_indices.size());
}

VoxelStreamRegionFiles::VoxelStreamRegionFiles() {
	_meta.version = FORMAT_VERSION;
	_meta.block_size_po2 = 4;
	_meta.region_size_po2 = 4;
	_meta.sector_size = 512; // next_power_of_2(_meta.block_size.volume() / 10) // based on compression ratios
	_meta.lod_count = 1;
	fill(_meta.channel_depths, VoxelBufferInternal::DEFAULT_CHANNEL_DEPTH);
	_meta.channel_depths[VoxelBufferInternal::CHANNEL_TYPE] = VoxelBufferInternal::DEFAULT_TYPE_CHANNEL_DEPTH;
	_meta.channel_depths[VoxelBufferInternal::CHANNEL_SDF] = VoxelBufferInternal::DEFAULT_SDF_CHANNEL_DEPTH;
	_meta.channel_depths[VoxelBufferInternal::CHANNEL_INDICES] = VoxelBufferInternal::DEFAULT_INDICES_CHANNEL_DEPTH;
	_meta.channel_depths[VoxelBufferInternal::CHANNEL_WEIGHTS] = VoxelBufferInternal::DEFAULT_WEIGHTS_CHANNEL_DEPTH;
}

VoxelStreamRegionFiles::~VoxelStreamRegionFiles() {
	close_all_regions();
}

void VoxelStreamRegionFiles::load_voxel_block(VoxelStream::VoxelQueryData &query) {
	load_voxel_blocks(Span<VoxelStream::VoxelQueryData>(&query, 1));
}

void VoxelStreamRegionFiles::save_voxel_block(VoxelStream::VoxelQueryData &query) {
	save_voxel_blocks(Span<VoxelStream::VoxelQueryData>(&query, 1));
}

void VoxelStreamRegionFiles::load_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) {
	ZN_PROFILE_SCOPE();

	// In order to minimize opening/closing files, requests are grouped according to their region.

	// Had to copy input to sort it, as some areas in the module break if they get responses in different order
	std::vector<unsigned int> sorted_block_indices;
	BlockQueryComparator comparator;
	comparator.self = this;
	get_sorted_indices(p_blocks, comparator, sorted_block_indices);

	for (unsigned int i = 0; i < sorted_block_indices.size(); ++i) {
		const unsigned int bi = sorted_block_indices[i];
		VoxelStream::VoxelQueryData &q = p_blocks[bi];
		const EmergeResult result = _load_block(q.voxel_buffer, q.origin_in_voxels, q.lod);
		switch (result) {
			case EMERGE_OK:
				q.result = RESULT_BLOCK_FOUND;
				break;
			case EMERGE_OK_FALLBACK:
				q.result = RESULT_BLOCK_NOT_FOUND;
				break;
			case EMERGE_FAILED:
				q.result = RESULT_ERROR;
				break;
			default:
				CRASH_NOW();
				break;
		}
	}
}

void VoxelStreamRegionFiles::save_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) {
	ZN_PROFILE_SCOPE();

	// Had to copy input to sort it, as some areas in the module break if they get responses in different order
	std::vector<unsigned int> sorted_block_indices;
	BlockQueryComparator comparator;
	comparator.self = this;
	get_sorted_indices(p_blocks, comparator, sorted_block_indices);

	for (unsigned int i = 0; i < sorted_block_indices.size(); ++i) {
		const unsigned int bi = sorted_block_indices[i];
		VoxelStream::VoxelQueryData &q = p_blocks[bi];
		_save_block(q.voxel_buffer, q.origin_in_voxels, q.lod);
	}
}

int VoxelStreamRegionFiles::get_used_channels_mask() const {
	// Assuming all, since that stream can store anything.
	return VoxelBufferInternal::ALL_CHANNELS_MASK;
}

VoxelStreamRegionFiles::EmergeResult VoxelStreamRegionFiles::_load_block(
		VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) {
	ZN_PROFILE_SCOPE();

	MutexLock lock(_mutex);

	if (_directory_path.is_empty()) {
		return EMERGE_OK_FALLBACK;
	}

	if (!_meta_loaded) {
		const FileResult load_res = load_meta();
		if (load_res != FILE_OK) {
			// No block was ever saved
			return EMERGE_OK_FALLBACK;
		}
	}

	const Vector3i block_size = Vector3iUtil::create(1 << _meta.block_size_po2);
	const Vector3i region_size = Vector3iUtil::create(1 << _meta.region_size_po2);

	CRASH_COND(!_meta_loaded);
	ERR_FAIL_COND_V(lod >= _meta.lod_count, EMERGE_FAILED);
	ERR_FAIL_COND_V(block_size != out_buffer.get_size(), EMERGE_FAILED);

	// Configure depths, as they might not be specified in old block data.
	// Regions are expected to contain such depths, and use those in the buffer to know how much data to read.
	for (unsigned int channel_index = 0; channel_index < _meta.channel_depths.size(); ++channel_index) {
		out_buffer.set_channel_depth(channel_index, _meta.channel_depths[channel_index]);
	}

	const Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	const Vector3i region_pos = get_region_position_from_blocks(block_pos);

	CachedRegion *cache = open_region(region_pos, lod, false);
	if (cache == nullptr || !cache->file_exists) {
		return EMERGE_OK_FALLBACK;
	}

	const Vector3i block_rpos = math::wrap(block_pos, region_size);

	const Error err = cache->region.load_block(block_rpos, out_buffer);
	switch (err) {
		case OK:
			return EMERGE_OK;

		case ERR_DOES_NOT_EXIST:
			return EMERGE_OK_FALLBACK;

		default:
			return EMERGE_FAILED;
	}
}

void VoxelStreamRegionFiles::_save_block(VoxelBufferInternal &voxel_buffer, Vector3i origin_in_voxels, int lod) {
	ZN_PROFILE_SCOPE();

	MutexLock lock(_mutex);

	ERR_FAIL_COND(_directory_path.is_empty());

	if (!_meta_loaded) {
		// If it's not loaded, always try to load meta file first if it exists already,
		// because we could want to save blocks without reading any
		FileResult load_res = load_meta();
		if (load_res != FILE_OK && load_res != FILE_CANT_OPEN) {
			// The file is present but there is a problem with it
			String meta_path = _directory_path.path_join(META_FILE_NAME);
			ERR_PRINT(String("Could not read {0}: error {1}").format(varray(meta_path, zylann::to_string(load_res))));
			return;
		}
	}

	if (!_meta_saved) {
		// First time we save the meta file, initialize it from the first block format
		for (unsigned int i = 0; i < _meta.channel_depths.size(); ++i) {
			_meta.channel_depths[i] = voxel_buffer.get_channel_depth(i);
		}
		FileResult err = save_meta();
		ERR_FAIL_COND(err != FILE_OK);
	}

	// Verify format
	const Vector3i block_size = Vector3iUtil::create(1 << _meta.block_size_po2);
	ERR_FAIL_COND(voxel_buffer.get_size() != block_size);
	for (unsigned int i = 0; i < VoxelBufferInternal::MAX_CHANNELS; ++i) {
		ERR_FAIL_COND(voxel_buffer.get_channel_depth(i) != _meta.channel_depths[i]);
	}

	const Vector3i region_size = Vector3iUtil::create(1 << _meta.region_size_po2);
	Vector3i block_pos = get_block_position_from_voxels(origin_in_voxels) >> lod;
	Vector3i region_pos = get_region_position_from_blocks(block_pos);
	Vector3i block_rpos = math::wrap(block_pos, region_size);

	CachedRegion *cache = open_region(region_pos, lod, true);
	ERR_FAIL_COND_MSG(cache == nullptr, "Could not save region file data");
	ERR_FAIL_COND(cache->region.save_block(block_rpos, voxel_buffer) != OK);
}

String VoxelStreamRegionFiles::get_directory() const {
	MutexLock lock(_mutex);
	return _directory_path;
}

void VoxelStreamRegionFiles::set_directory(String dirpath) {
	MutexLock lock(_mutex);
	if (_directory_path != dirpath) {
		close_all_regions();
		_directory_path = dirpath.strip_edges();
		_meta_loaded = false;
		_meta_saved = false;
		load_meta();
		notify_property_list_changed();
	}
}

static bool u8_from_json_variant(const Variant &v, uint8_t &i) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT && v.get_type() != Variant::FLOAT, false);
	int n = v;
	ERR_FAIL_COND_V(n < 0 || n > 255, false);
	// Constructing an `int` first because when compiling with GodotCpp, the conversion isn't working directly
	i = int(v);
	return true;
}

static bool u32_from_json_variant(const Variant &v, uint32_t &i) {
	ERR_FAIL_COND_V(v.get_type() != Variant::INT && v.get_type() != Variant::FLOAT, false);
	ERR_FAIL_COND_V(v.operator int64_t() < 0, false);
	i = v;
	return true;
}

static bool depth_from_json_variant(Variant &v, VoxelBufferInternal::Depth &d) {
	uint8_t n;
	ERR_FAIL_COND_V(!u8_from_json_variant(v, n), false);
	ZN_ASSERT_RETURN_V(n < VoxelBufferInternal::DEPTH_COUNT, false);
	d = (VoxelBufferInternal::Depth)n;
	return true;
}

FileResult VoxelStreamRegionFiles::save_meta() {
	ERR_FAIL_COND_V(_directory_path == "", FILE_CANT_OPEN);

	Dictionary d;
	d["version"] = _meta.version;
	d["block_size_po2"] = _meta.block_size_po2;
	d["region_size_po2"] = _meta.region_size_po2;
	d["lod_count"] = _meta.lod_count;
	d["sector_size"] = _meta.sector_size;

	Array channel_depths;
	channel_depths.resize(_meta.channel_depths.size());
	for (unsigned int i = 0; i < _meta.channel_depths.size(); ++i) {
		channel_depths[i] = _meta.channel_depths[i];
	}
	d["channel_depths"] = channel_depths;

	const String json_string = JSON::stringify(d, "\t", true);

	// Make sure the directory exists
	{
		const CharString directory_path_utf8 = _directory_path.utf8();
		Error err = check_directory_created_using_file_locker(directory_path_utf8.get_data());
		if (err != OK) {
			ERR_PRINT("Could not save meta");
			return FILE_CANT_OPEN;
		}
	}

	const String meta_path = _directory_path.path_join(META_FILE_NAME);
	const CharString meta_path_utf8 = meta_path.utf8();

	Error err;
	VoxelFileLockerWrite file_wlock(meta_path_utf8.get_data());
	Ref<FileAccess> f = open_file(meta_path, FileAccess::WRITE, err);
	if (f.is_null()) {
		ERR_PRINT(String("Could not save {0}").format(varray(meta_path)));
		return FILE_CANT_OPEN;
	}

	f->store_string(json_string);

	_meta_saved = true;
	_meta_loaded = true;

	return FILE_OK;
}

static void migrate_region_meta_data(Dictionary &data) {
	if (data["version"] == Variant(real_t(FORMAT_VERSION_LEGACY_1))) {
		Array depths;
		depths.resize(VoxelBufferInternal::MAX_CHANNELS);
		for (int i = 0; i < depths.size(); ++i) {
			depths[i] = VoxelBufferInternal::DEFAULT_CHANNEL_DEPTH;
		}
		data["channel_depths"] = depths;
		data["version"] = FORMAT_VERSION_LEGACY_2;
	}

	if (data["version"] == Variant(real_t(FORMAT_VERSION_LEGACY_2))) {
		// Nothing for the region forest, but indicates region files may be upgraded to v3.
		data["version"] = FORMAT_VERSION;
	}

	// if (data["version"] != Variant(real_t(FORMAT_VERSION))) {
	//  TODO Throw error?
	// }
}

FileResult VoxelStreamRegionFiles::load_meta() {
	ERR_FAIL_COND_V(_directory_path == "", FILE_CANT_OPEN);

	// Ensure you cleanup previous world before loading another
	CRASH_COND(_region_cache.size() > 0);

	const String meta_path = _directory_path.path_join(META_FILE_NAME);
	String json_string;

	{
		Error err;
		const CharString meta_path_utf8 = meta_path.utf8();
		VoxelFileLockerRead file_rlock(meta_path_utf8.get_data());
		Ref<FileAccess> f = open_file(meta_path, FileAccess::READ, err);
		if (f.is_null()) {
			return FILE_CANT_OPEN;
		}
		json_string = get_as_text(**f);
	}

	// Note: I chose JSON purely for debugging purposes. This file is not meant to be edited by hand.
	// World configuration changes may need a full converter.

	Ref<JSON> json;
	json.instantiate();
	const Error json_err = json->parse(json_string);
	Variant res = json->get_data();
	if (json_err != OK) {
		const String json_err_msg = json->get_error_message();
		const int json_err_line = json->get_error_line();
		ZN_PRINT_ERROR(format("Error when parsing {}: line {}: {}", meta_path, json_err_line, json_err_msg));
		return FILE_INVALID_DATA;
	}

	Dictionary d = res;
	migrate_region_meta_data(d);
	Meta meta;
	ERR_FAIL_COND_V(!u8_from_json_variant(d["version"], meta.version), FILE_INVALID_DATA);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["block_size_po2"], meta.block_size_po2), FILE_INVALID_DATA);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["region_size_po2"], meta.region_size_po2), FILE_INVALID_DATA);
	ERR_FAIL_COND_V(!u8_from_json_variant(d["lod_count"], meta.lod_count), FILE_INVALID_DATA);
	ERR_FAIL_COND_V(!u32_from_json_variant(d["sector_size"], meta.sector_size), FILE_INVALID_DATA);

	ERR_FAIL_COND_V(meta.version < 0, FILE_INVALID_DATA);

	Array channel_depths_data = d["channel_depths"];
	ERR_FAIL_COND_V(channel_depths_data.size() != VoxelBufferInternal::MAX_CHANNELS, FILE_INVALID_DATA);
	for (int i = 0; i < channel_depths_data.size(); ++i) {
		ERR_FAIL_COND_V(!depth_from_json_variant(channel_depths_data[i], meta.channel_depths[i]), FILE_INVALID_DATA);
	}

	ERR_FAIL_COND_V(!check_meta(meta), FILE_INVALID_DATA);

	_meta = meta;
	_meta_loaded = true;
	_meta_saved = true;

	return FILE_OK;
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
	a[4] = RegionFormat::FILE_EXTENSION;
	return _directory_path.path_join(String("regions/lod{0}/r.{1}.{2}.{3}.{4}").format(a));
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

VoxelStreamRegionFiles::CachedRegion *VoxelStreamRegionFiles::open_region(
		const Vector3i region_pos, unsigned int lod, bool create_if_not_found) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(!_meta_loaded, nullptr);
	ZN_ASSERT_RETURN_V(lod < constants::MAX_LOD, nullptr);

	CachedRegion *cached_region = get_region_from_cache(region_pos, lod);
	if (cached_region != nullptr) {
		return cached_region;
	}

	while (_region_cache.size() > _max_open_regions - 1) {
		close_oldest_region();
	}
	// Not in cache, we'll have to open or create it

	String fpath = get_region_file_path(region_pos, lod);

	cached_region = memnew(CachedRegion);

	// Configure format because we might have to create the file, and some old file versions don't embed format
	{
		RegionFormat format;
		format.block_size_po2 = _meta.block_size_po2;
		format.channel_depths = _meta.channel_depths;
		// TODO Palette support
		format.has_palette = false;
		format.region_size = Vector3iUtil::create(1 << _meta.region_size_po2);
		format.sector_size = _meta.sector_size;

		cached_region->region.set_format(format);
		cached_region->position = region_pos;
		cached_region->lod = lod;
	}

	const Error err = cached_region->region.open(fpath, create_if_not_found);

	// Things we could do for optimization:
	// - Cache the fact the file doesn't exist, so we won't need to do a system call to actually check it every time.
	// - No need to read the header again when it has been read once,
	//   we assume no other process will modify region files.

	if (err != OK) {
		memdelete(cached_region);
		if (create_if_not_found) {
			// Could not create it apparently
			ERR_PRINT(String("Could not open or create region file {0}, error: {1}").format(varray(fpath, err)));
			return nullptr;
		} else {
			// Does not exist, it was probably expected
			return nullptr;
		}
	}

	// Make sure it has correct format
	{
		const RegionFormat &format = cached_region->region.get_format();
		if (format.block_size_po2 != _meta.block_size_po2 //
				|| format.channel_depths != _meta.channel_depths //
				|| format.region_size != Vector3iUtil::create(1 << _meta.region_size_po2) //
				|| format.sector_size != _meta.sector_size) {
			ERR_PRINT("Region file has unexpected format");
			memdelete(cached_region);
			return nullptr;
		}
	}

	// TODO Debug check to make sure we did not already cache it
	_region_cache.push_back(cached_region);

	cached_region->file_exists = true;
	cached_region->last_opened = Time::get_singleton()->get_ticks_usec();

	return cached_region;
}

// TODO Get rid of to simplify?
void VoxelStreamRegionFiles::close_region(CachedRegion *region) {
	region->region.close();
}

void VoxelStreamRegionFiles::close_oldest_region() {
	// Close region assumed to be the least recently used

	if (_region_cache.size() == 0) {
		return;
	}

	int oldest_index = -1;
	uint64_t oldest_time = 0;
	const uint64_t now = Time::get_singleton()->get_ticks_usec();

	for (unsigned int i = 0; i < _region_cache.size(); ++i) {
		const CachedRegion *r = _region_cache[i];
		const uint64_t time = now - r->last_opened;
		if (time >= oldest_time) {
			oldest_index = i;
		}
	}

	CachedRegion *region = _region_cache[oldest_index];
	_region_cache.erase(_region_cache.begin() + oldest_index);

	close_region(region);
	memdelete(region);
}

static inline int convert_block_coordinate(int p_x, int old_size, int new_size) {
	return math::floordiv(p_x * old_size, new_size);
}

static Vector3i convert_block_coordinates(Vector3i pos, Vector3i old_size, Vector3i new_size) {
	return Vector3i(convert_block_coordinate(pos.x, old_size.x, new_size.x),
			convert_block_coordinate(pos.y, old_size.y, new_size.y),
			convert_block_coordinate(pos.z, old_size.z, new_size.z));
}

void VoxelStreamRegionFiles::_convert_files(Meta new_meta) {
	// TODO Converting across different block sizes is untested.
	// I wrote it because it would be too bad to loose large voxel worlds because of a setting change, so one day we may
	// need it

	ZN_PRINT_VERBOSE("Converting region files");
	// This can be a very long and slow operation. Better run this in a thread.

	ERR_FAIL_COND(!_meta_saved);
	ERR_FAIL_COND(!_meta_loaded);

	close_all_regions();

	Ref<VoxelStreamRegionFiles> old_stream;
	old_stream.instantiate();
	// Keep file cache to a minimum for the old stream, we'll query all blocks once anyways
	old_stream->_max_open_regions = MAX(1, FOPEN_MAX);

	// Backup current folder by renaming it, leaving the current name vacant
	{
		Ref<DirAccess> da = open_directory(_directory_path);
		ERR_FAIL_COND(da.is_null());
		int i = 0;
		String old_dir;
		while (true) {
			if (i == 0) {
				old_dir = _directory_path + "_old";
			} else {
				old_dir = _directory_path + "_old" + String::num_int64(i);
			}
			if (directory_exists(**da, old_dir)) {
				++i;
			} else {
				Error err = da->rename(_directory_path, old_dir);
				ERR_FAIL_COND_MSG(err != OK,
						String("Failed to rename '{0}' to '{1}', error {2}")
								.format(varray(_directory_path, old_dir, err)));
				break;
			}
		}

		old_stream->set_directory(old_dir);
		ZN_PRINT_VERBOSE(format("Data backed up as {}", old_dir));
	}

	struct PositionAndLod {
		Vector3i position;
		int lod;
	};

	ERR_FAIL_COND(old_stream->load_meta() != FILE_OK);

	std::vector<PositionAndLod> old_region_list;
	Meta old_meta = old_stream->_meta;

	// Get list of all regions from the old stream
	{
		for (int lod = 0; lod < old_meta.lod_count; ++lod) {
			const String lod_folder =
					old_stream->_directory_path.path_join("regions").path_join("lod") + String::num_int64(lod);
			const String ext = String(".") + RegionFormat::FILE_EXTENSION;

			Ref<DirAccess> da = open_directory(lod_folder);
			if (da.is_null()) {
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
					PackedStringArray parts = fname.split(".");
					// r.x.y.z.ext
					ERR_FAIL_COND_MSG(
							parts.size() < 4, String("Found invalid region file: '{0}'").format(varray(fname)));
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
	ERR_FAIL_COND(save_meta() != FILE_OK);

	const Vector3i old_block_size = Vector3iUtil::create(1 << old_meta.block_size_po2);
	const Vector3i new_block_size = Vector3iUtil::create(1 << _meta.block_size_po2);

	const Vector3i old_region_size = Vector3iUtil::create(1 << old_meta.region_size_po2);

	// Read all blocks from the old stream and write them into the new one

	for (unsigned int i = 0; i < old_region_list.size(); ++i) {
		PositionAndLod region_info = old_region_list[i];

		const CachedRegion *old_region = old_stream->open_region(region_info.position, region_info.lod, false);
		if (old_region == nullptr) {
			continue;
		}

		ZN_PRINT_VERBOSE(format("Converting region lod{}/{}", region_info.lod, region_info.position));

		const unsigned int blocks_count = old_region->region.get_header_block_count();
		for (unsigned int j = 0; j < blocks_count; ++j) {
			if (!old_region->region.has_block(j)) {
				continue;
			}

			VoxelBufferInternal old_block;
			old_block.create(old_block_size.x, old_block_size.y, old_block_size.z);

			VoxelBufferInternal new_block;
			new_block.create(new_block_size.x, new_block_size.y, new_block_size.z);

			// Load block from old stream
			Vector3i block_rpos = old_region->region.get_block_position_from_index(j);
			Vector3i block_pos = block_rpos + region_info.position * old_region_size;
			VoxelStream::VoxelQueryData old_block_load_query{
				old_block, //
				block_pos * old_block_size << region_info.lod, //
				region_info.lod, //
				RESULT_ERROR //
			};
			old_stream->load_voxel_block(old_block_load_query);

			// Save it in the new one
			if (old_block_size == new_block_size) {
				VoxelStream::VoxelQueryData old_block_save_query{
					old_block, //
					block_pos * new_block_size << region_info.lod, //
					region_info.lod,
					RESULT_ERROR //
				};
				save_voxel_block(old_block_save_query);

			} else {
				Vector3i new_block_pos = convert_block_coordinates(block_pos, old_block_size, new_block_size);

				// TODO Support any size? Assuming cubic blocks here
				if (old_block_size.x < new_block_size.x) {
					Vector3i ratio = new_block_size / old_block_size;
					Vector3i rel = block_pos % ratio;

					// Copy to a sub-area of one block
					VoxelStream::VoxelQueryData new_block_load_query{ //
						new_block, //
						new_block_pos * new_block_size << region_info.lod, //
						region_info.lod, //
						RESULT_ERROR
					};
					load_voxel_block(new_block_load_query);

					Vector3i dst_pos = rel * old_block.get_size();

					for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS;
							++channel_index) {
						new_block.copy_from(old_block, Vector3i(), old_block.get_size(), dst_pos, channel_index);
					}

					new_block.compress_uniform_channels();
					VoxelStream::VoxelQueryData new_block_save_query{ //
						new_block, //
						new_block_pos * new_block_size << region_info.lod, //
						region_info.lod, //
						RESULT_ERROR
					};
					save_voxel_block(new_block_save_query);

				} else {
					// Copy to multiple blocks
					Vector3i area = new_block_size / old_block_size;
					Vector3i rpos;

					for (rpos.z = 0; rpos.z < area.z; ++rpos.z) {
						for (rpos.x = 0; rpos.x < area.x; ++rpos.x) {
							for (rpos.y = 0; rpos.y < area.y; ++rpos.y) {
								Vector3i src_min = rpos * new_block.get_size();
								Vector3i src_max = src_min + new_block.get_size();

								for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS;
										++channel_index) {
									new_block.copy_from(old_block, src_min, src_max, Vector3i(), channel_index);
								}

								VoxelStream::VoxelQueryData new_block_save_query{ //
									new_block, //
									(new_block_pos + rpos) * new_block_size << region_info.lod, //
									region_info.lod, //
									RESULT_ERROR
								};
								save_voxel_block(new_block_save_query);
							}
						}
					}
				}
			}
		}
	}

	close_all_regions();

	ZN_PRINT_VERBOSE("Done converting region files");
}

Vector3i VoxelStreamRegionFiles::get_region_size() const {
	MutexLock lock(_mutex);
	return Vector3iUtil::create(1 << _meta.region_size_po2);
}

Vector3 VoxelStreamRegionFiles::get_region_size_v() const {
	return get_region_size();
}

int VoxelStreamRegionFiles::get_region_size_po2() const {
	MutexLock lock(_mutex);
	return _meta.region_size_po2;
}

int VoxelStreamRegionFiles::get_block_size_po2() const {
	MutexLock lock(_mutex);
	return _meta.block_size_po2;
}

int VoxelStreamRegionFiles::get_lod_count() const {
	MutexLock lock(_mutex);
	return _meta.lod_count;
}

int VoxelStreamRegionFiles::get_sector_size() const {
	MutexLock lock(_mutex);
	return _meta.sector_size;
}

// TODO The following settings are hard to change.
// If files already exist, these settings will be ignored.
// To be applied, files either need to be wiped out or converted, which is a super-heavy operation.
// This can be made easier by adding a button to the inspector to convert existing files just in case

void VoxelStreamRegionFiles::set_region_size_po2(int p_region_size_po2) {
	{
		MutexLock lock(_mutex);
		if (_meta.region_size_po2 == p_region_size_po2) {
			return;
		}
		ERR_FAIL_COND_MSG(
				_meta_loaded, "Can't change existing region size without heavy conversion. Use convert_files().");
		ERR_FAIL_COND(p_region_size_po2 < 1);
		ERR_FAIL_COND(p_region_size_po2 > 8);
		_meta.region_size_po2 = p_region_size_po2;
	}
	emit_changed();
}

void VoxelStreamRegionFiles::set_block_size_po2(int p_block_size_po2) {
	{
		MutexLock lock(_mutex);
		if (_meta.block_size_po2 == p_block_size_po2) {
			return;
		}
		ERR_FAIL_COND_MSG(
				_meta_loaded, "Can't change existing block size without heavy conversion. Use convert_files().");
		ERR_FAIL_COND(p_block_size_po2 < 1);
		ERR_FAIL_COND(p_block_size_po2 > 8);
		_meta.block_size_po2 = p_block_size_po2;
	}
	emit_changed();
}

void VoxelStreamRegionFiles::set_sector_size(int p_sector_size) {
	{
		MutexLock lock(_mutex);
		if (static_cast<int>(_meta.sector_size) == p_sector_size) {
			return;
		}
		ERR_FAIL_COND_MSG(
				_meta_loaded, "Can't change existing sector size without heavy conversion. Use convert_files().");
		ERR_FAIL_COND(p_sector_size < 256);
		ERR_FAIL_COND(p_sector_size > 65536);
		_meta.sector_size = p_sector_size;
	}
	emit_changed();
}

void VoxelStreamRegionFiles::set_lod_count(int p_lod_count) {
	{
		MutexLock lock(_mutex);
		if (_meta.lod_count == p_lod_count) {
			return;
		}
		ERR_FAIL_COND_MSG(
				_meta_loaded, "Can't change existing LOD count without heavy conversion. Use convert_files().");
		ERR_FAIL_COND(p_lod_count < 1);
		ERR_FAIL_COND(p_lod_count > 32);
		_meta.lod_count = p_lod_count;
	}
	emit_changed();
}

void VoxelStreamRegionFiles::convert_files(Dictionary d) {
	Meta meta;
	meta.version = _meta.version;
	meta.block_size_po2 = int(d["block_size_po2"]);
	meta.region_size_po2 = int(d["region_size_po2"]);
	meta.sector_size = int(d["sector_size"]);
	meta.lod_count = int(d["lod_count"]);

	{
		MutexLock lock(_mutex);

		ERR_FAIL_COND_MSG(!check_meta(meta), "Invalid setting");

		if (!_meta_loaded) {
			if (load_meta() != FILE_OK) {
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
	}

	emit_changed();
}

void VoxelStreamRegionFiles::flush() {
	ZN_PROFILE_SCOPE();
	MutexLock lock(_mutex);
	for (CachedRegion *cr : _region_cache) {
		cr->region.flush();
	}
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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "block_size_po2"), "set_block_size_po2", "get_block_size_po2");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sector_size"), "set_sector_size", "get_sector_size");
}

} // namespace zylann::voxel
