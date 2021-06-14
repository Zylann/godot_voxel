#ifndef VOXEL_STREAM_REGION_H
#define VOXEL_STREAM_REGION_H

#include "../../util/fixed_array.h"
#include "../file_utils.h"
#include "../voxel_block_serializer.h"
#include "../voxel_stream.h"
#include "region_file.h"

class FileAccess;

// TODO Rename VoxelStreamRegionForest

// Loads and saves blocks to the filesystem, in multiple region files indexed by world position, under a directory.
// Loading and saving blocks in batches of similar regions makes a lot more sense here,
// because it allows to keep using the same file handles and avoid switching.
// Inspired by https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game
//
// Region files are not thread-safe. Because of this, internal mutexing may often constrain the use by one thread only.
//
class VoxelStreamRegionFiles : public VoxelStream {
	GDCLASS(VoxelStreamRegionFiles, VoxelStream)
public:
	VoxelStreamRegionFiles();
	~VoxelStreamRegionFiles();

	Result emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) override;
	void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) override;

	void emerge_blocks(Vector<VoxelBlockRequest> &p_blocks, Vector<Result> &out_results) override;
	void immerge_blocks(const Vector<VoxelBlockRequest> &p_blocks) override;

	int get_used_channels_mask() const override;

	String get_directory() const;
	void set_directory(String dirpath);

	Vector3i get_region_size() const;
	Vector3 get_region_size_v() const;
	int get_region_size_po2() const;

	int get_sector_size() const;

	int get_block_size_po2() const override;
	int get_lod_count() const override;

	void set_block_size_po2(int p_block_size_po2);
	void set_region_size_po2(int p_region_size_po2);
	void set_sector_size(int p_sector_size);
	void set_lod_count(int p_lod_count);

	void convert_files(Dictionary d);

protected:
	static void _bind_methods();

private:
	struct CachedRegion;
	struct RegionHeader;

	// TODO Redundant with VoxelStream::Result. May be replaced
	enum EmergeResult {
		EMERGE_OK,
		EMERGE_OK_FALLBACK,
		EMERGE_FAILED
	};

	EmergeResult _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);
	void _immerge_block(Ref<VoxelBuffer> voxel_buffer, Vector3i origin_in_voxels, int lod);

	VoxelFileResult save_meta();
	VoxelFileResult load_meta();
	Vector3i get_block_position_from_voxels(const Vector3i &origin_in_voxels) const;
	Vector3i get_region_position_from_blocks(const Vector3i &block_position) const;
	void close_all_regions();
	String get_region_file_path(const Vector3i &region_pos, unsigned int lod) const;
	CachedRegion *open_region(const Vector3i region_pos, unsigned int lod, bool create_if_not_found);
	void close_region(CachedRegion *cache);
	CachedRegion *get_region_from_cache(const Vector3i pos, int lod) const;
	int get_sectors_count(const RegionHeader &header) const;
	void close_oldest_region();

	struct Meta {
		uint8_t version = -1;
		uint8_t lod_count = 0;
		uint8_t block_size_po2 = 0; // How many voxels in a cubic block
		uint8_t region_size_po2 = 0; // How many blocks in one cubic region
		FixedArray<VoxelBuffer::Depth, VoxelBuffer::MAX_CHANNELS> channel_depths;
		uint32_t sector_size = 0; // Blocks are stored at offsets multiple of that size
	};

	static bool check_meta(const Meta &meta);
	void _convert_files(Meta new_meta);

	// Orders block requests so those querying the same regions get grouped together
	struct BlockRequestComparator {
		VoxelStreamRegionFiles *self = nullptr;

		// operator<
		_FORCE_INLINE_ bool operator()(const VoxelBlockRequest &a, const VoxelBlockRequest &b) const {
			if (a.lod < b.lod) {
				return true;
			} else if (a.lod > b.lod) {
				return false;
			}
			Vector3i bpos_a = self->get_block_position_from_voxels(a.origin_in_voxels);
			Vector3i bpos_b = self->get_block_position_from_voxels(b.origin_in_voxels);
			Vector3i rpos_a = self->get_region_position_from_blocks(bpos_a);
			Vector3i rpos_b = self->get_region_position_from_blocks(bpos_b);
			return rpos_a < rpos_b;
		}
	};

	static thread_local VoxelBlockSerializerInternal _block_serializer;

	// TODO This is not thread-friendly.
	// `VoxelRegionFile` is not thread-safe so we have to limit the usage to one thread at once, blocking the others.
	// A refactoring should be done to allow better threading.

	struct CachedRegion {
		Vector3i position;
		int lod = 0;
		bool file_exists = false;
		VoxelRegionFile region;
		uint64_t last_opened = 0;
		//uint64_t last_accessed;
	};

	String _directory_path;
	Meta _meta;
	bool _meta_loaded = false;
	bool _meta_saved = false;
	std::vector<CachedRegion *> _region_cache;
	// TODO Add memory caches to increase capacity.
	unsigned int _max_open_regions = MIN(8, FOPEN_MAX);

	Mutex _mutex;
};

#endif // VOXEL_STREAM_REGION_H
