#ifndef VOXEL_DATA_H
#define VOXEL_DATA_H

#include "../generators/voxel_generator.h"
#include "../modifiers/voxel_modifier_stack.h"
#include "../streams/voxel_stream.h"
#include "../util/thread/mutex.h"
#include "../util/thread/spatial_lock_3d.h"
#include "voxel_data_map.h"

namespace zylann::voxel {

class VoxelDataGrid;

// Generic storage containing everything needed to access voxel data.
// Contains edits, procedural sources and file stream so voxels not physically stored in memory can be obtained.
// This does not contain meshing or instancing information, only voxels.
// Individual calls should be thread-safe.
class VoxelData {
public:
	VoxelData();
	~VoxelData();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Configuration.
	// Changing these settings while data is already loaded can be expensive, or cause data to be reset.
	// If threaded tasks are still working on the data while this happens, they should be cancelled or ignored.

	inline unsigned int get_block_size() const {
		return _lods[0].map.get_block_size();
	}

	inline unsigned int get_block_size_po2() const {
		return _lods[0].map.get_block_size_pow2();
	}

	inline Vector3i voxel_to_block(Vector3i pos) const {
		return _lods[0].map.voxel_to_block(pos);
	}

	inline Vector3i block_to_voxel(Vector3i pos) const {
		return _lods[0].map.block_to_voxel(pos);
	}

	void set_lod_count(unsigned int p_lod_count);

	// Clears voxel data. Keeps modifiers, generator and settings.
	void reset_maps();

	inline unsigned int get_lod_count() const {
		MutexLock rlock(_settings_mutex);
		return _lod_count;
	}

	void set_bounds(Box3i bounds);

	inline Box3i get_bounds() const {
		MutexLock rlock(_settings_mutex);
		return _bounds_in_voxels;
	}

	void set_generator(Ref<VoxelGenerator> generator);

	inline Ref<VoxelGenerator> get_generator() const {
		MutexLock rlock(_settings_mutex);
		return _generator;
	}

	void set_stream(Ref<VoxelStream> stream);

	inline Ref<VoxelStream> get_stream() const {
		MutexLock rlock(_settings_mutex);
		return _stream;
	}

	inline VoxelModifierStack &get_modifiers() {
		return _modifiers;
	}

	inline const VoxelModifierStack &get_modifiers() const {
		return _modifiers;
	}

	void set_streaming_enabled(bool enabled);

	inline bool is_streaming_enabled() const {
		return _streaming_enabled;
	}

	void set_full_load_completed(bool complete);

	inline bool is_full_load_completed() const {
		return _full_load_completed;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Voxel queries.
	// When not specified, the used LOD index is 0.

	VoxelSingleValue get_voxel(Vector3i pos, unsigned int channel_index, VoxelSingleValue defval) const;
	bool try_set_voxel(uint64_t value, Vector3i pos, unsigned int channel_index);

	float get_voxel_f(Vector3i pos, unsigned int channel_index) const;
	bool try_set_voxel_f(real_t value, Vector3i pos, unsigned int channel_index);

	// Copies voxel data in a box from LOD0.
	// `channels_mask` bits tell which channel is read.
	void copy(Vector3i min_pos, VoxelBuffer &dst_buffer, unsigned int channels_mask) const;

	// Pastes voxel data in a box at LOD0.
	// `channels_mask` bits tell which channel is pasted.
	// If `use_mask` is used, will only write voxels of the source buffer that are not equal to `mask_value`.
	// If `create_new_blocks` is true, blocks will be created if not found in the area.
	void paste(Vector3i min_pos, const VoxelBuffer &src_buffer, unsigned int channels_mask, bool create_new_blocks);

	void paste_masked(Vector3i min_pos, const VoxelBuffer &src_buffer, unsigned int channels_mask, uint8_t mask_channel,
			uint64_t mask_value, bool create_new_blocks);

	// Tests if the given area is loaded at LOD0.
	// This is necessary for editing destructively.
	bool is_area_loaded(const Box3i p_voxels_box) const;

	// Generates all non-present blocks in preparation for an edit.
	// Every block intersecting with the box at every LOD will be checked.
	// This function runs sequentially and should be thread-safe. May be used if blocks are immediately needed.
	// It will block if other threads are accessing the same data.
	// When streaming is enabled, non-loaded areas will not be touched.
	// WARNING: this does not check if the area is editable.
	void pre_generate_box(Box3i voxel_box);

	// Clears voxel data from blocks that are pure results of generators and modifiers.
	// WARNING: this does not check if the area is editable.
	// TODO Rename `clear_cached_voxel_data_in_area`
	void clear_cached_blocks_in_voxel_area(Box3i p_voxel_box);

	// Flags all blocks in the given area as modified at LOD0.
	// Also marks them as requiring LOD updates (if lod count is 1 this has no effect).
	// Optionally, returns a list of affected block positions which did not require LOD updates before.
	void mark_area_modified(Box3i p_voxel_box, StdVector<Vector3i> *lod0_new_blocks_to_lod, bool require_lod_updates);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Block-aware API

	// Sets all the data of a block.
	// If the block already exists, returns false. Otherwise, returns true.
	bool try_set_block(Vector3i block_position, const VoxelDataBlock &block);

	// Sets all the data of a block.
	// If the block doesn't exist, it is added and the function returns true.
	// If the block already exists, `action_when_exists` is called and the function returns false.
	// `void action_when_exists(VoxelDataBlock &existing_block, const VoxelDataBlock &incoming_block)`
	template <typename F>
	bool try_set_block(Vector3i block_position, const VoxelDataBlock &block, F action_when_exists) {
		Lod &lod = _lods[block.get_lod_index()];
#ifdef DEBUG_ENABLED
		if (block.has_voxels()) {
			ZN_ASSERT(block.get_voxels_const().get_size() == Vector3iUtil::create(get_block_size()));
		}
#endif
		RWLockWrite wlock(lod.map_lock);
		VoxelDataBlock *existing_block = lod.map.get_block(block_position);
		if (existing_block != nullptr) {
			action_when_exists(*existing_block, block);
			return false;
		} else {
			lod.map.set_block(block_position, block);
			return true;
		}
	}

	template <typename F>
	void for_each_block_position(F op) const {
		const unsigned int lod_count = get_lod_count();
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const Lod &lod = _lods[lod_index];
			RWLockRead rlock(lod.map_lock);
			lod.map.for_each_block_position(op);
		}
	}

	// void op(Vector3i bpos, const VoxelDataBlock &block)
	// template <typename F>
	// void for_each_block_r(F op) const {
	// 	const unsigned int lod_count = get_lod_count();
	// 	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
	// 		const Lod &lod = _lods[lod_index];
	// 		SpatialLock3D::Read srlock(lod.spatial_lock, BoxBounds3i::from_everywhere());
	// 		RWLockRead rlock(lod.map_lock);
	// 		lod.map.for_each_block(op);
	// 	}
	// }

	// void op(Vector3i bpos, const VoxelDataBlock &block)
	template <typename F>
	void for_each_block_at_lod_r(F op, unsigned int lod_index) const {
		const Lod &lod = _lods[lod_index];
		SpatialLock3D::Read srlock(lod.spatial_lock, BoxBounds3i::from_everywhere());
		RWLockRead rlock(lod.map_lock);
		lod.map.for_each_block(op);
	}

	// Tests if a block exists at the specified block position and LOD index.
	// This is mainly used for debugging so it isn't optimal, don't use this if you plan to query many blocks.
	bool has_block(Vector3i bpos, unsigned int lod_index) const;

	// Tests if all blocks in an area are loaded. If any isn't, returns false. Otherwise, returns true.
	// Accounts for data boundaries, but is slower as a result.
	bool has_all_blocks_in_area(Box3i data_blocks_box, unsigned int lod_index) const;

	// Tests if all blocks in an area are loaded. If any isn't, returns false. Otherwise, returns true.
	// Doesn't account for data boundaries, so if the given box overlaps outside, it will return false.
	bool has_all_blocks_in_area_unbound(Box3i data_blocks_box, unsigned int lod_index) const;

	// Gets the total amount of allocated blocks. This includes blocks having no voxel data.
	unsigned int get_block_count() const;

	struct BlockLocation {
		Vector3i position;
		uint32_t lod_index;
	};

	// Updates the LODs of all blocks at given positions, and resets their flags telling that they need LOD updates.
	// Optionally, returns a list of affected block positions.
	void update_lods(Span<const Vector3i> modified_lod0_blocks, StdVector<BlockLocation> *out_updated_blocks);

	struct BlockToSave {
		std::shared_ptr<VoxelBuffer> voxels;
		Vector3i position;
		uint32_t lod_index;
	};

	// Unloads data blocks in the specified area. If some of them were modified and `to_save` is not null, their data
	// will be returned for the caller to save.
	void unload_blocks(Box3i bbox, unsigned int lod_index, StdVector<BlockToSave> *to_save);

	// Unloads data blocks at specified positions of LOD0. If some of them were modified and `to_save` is not null,
	// their data will be returned for the caller to save.
	// void unload_blocks(Span<const Vector3i> positions, StdVector<BlockToSave> *to_save);

	// If the block at the specified LOD0 position exists and is modified, marks it as non-modified and returns a copy
	// of its data to save. Returns true if there is something to save.
	bool consume_block_modifications(Vector3i bpos, BlockToSave &out_to_save);

	// Marks all modified blocks as unmodified and returns their data to save. if `with_copy` is true, the returned data
	// will be a copy, otherwise it will reference voxel data. Prefer using references when about to quit for example.
	void consume_all_modifications(StdVector<BlockToSave> &to_save, bool with_copy);

	// Gets missing blocks out of the given block positions.
	// WARNING: positions outside bounds will be considered missing too.
	// TODO Don't consider positions outside bounds to be missing? This is only a byproduct of migrating old
	// code. It doesn't check this because the code using this function already does it (a bit more efficiently,
	// but still).
	void get_missing_blocks(
			Span<const Vector3i> block_positions, unsigned int lod_index, StdVector<Vector3i> &out_missing) const;

	// Gets missing blocks out of the given area in block coordinates.
	// If the area intersects the outside of the bounds, it will be clipped.
	void get_missing_blocks(Box3i p_blocks_box, unsigned int lod_index, StdVector<Vector3i> &out_missing) const;

	// Gets blocks with voxel data in the given area in block coordinates.
	// Voxel data references are returned in an array big enough to contain a grid of the size of the area.
	// Blocks found will be placed at an index computed as if the array was a flat grid (ZXY).
	// Entries without voxel data will be left to null.
	void get_blocks_with_voxel_data(
			Box3i p_blocks_box, unsigned int lod_index, Span<std::shared_ptr<VoxelBuffer>> out_blocks) const;

	// Gets blocks with voxels at the given LOD and indexes them in a grid. This will query every location
	// intersecting the box at the specified LOD, so if the area is large, you may want to do a broad check first.
	// WARNING: data isn't locked, you have to keep a shared reference to VoxelData in order to use SpatialLock3D.
	void get_blocks_grid(VoxelDataGrid &grid, Box3i box_in_voxels, unsigned int lod_index) const;

	// TODO Areas that use this accessor might as well move their logic in this class
	SpatialLock3D &get_spatial_lock(unsigned int lod_index) const;

	// Tests the presence of edited blocks in the given area by looking up LOD mips. It can report false positives due
	// to the broad nature of the check, but runs a lot faster than a full test. This is only usable with volumes
	// using LOD mips (edited blocks have half-resolution counterparts all the way up to maximum LOD).

	bool has_blocks_with_voxels_in_area_broad_mip_test(Box3i box_in_voxels) const;

	// Access voxels of a specific block.
	// WARNING: you must hold the spatial lock before calling this, and until you're done working on such blocks.
	// Can return null.
	std::shared_ptr<VoxelBuffer> try_get_block_voxels(Vector3i bpos);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Reference-counted API (LOD0 only)
	// Data blocks have a reference count that can be optionally used.

	// Increases the reference count of loaded blocks in the area.
	// Returns positions where blocks were loaded, and where they were missing.
	// Shallow copies of found blocks are returned (voxel data is referenced).
	// Should only be used if refcounting is used, may fail otherwise.
	void view_area(Box3i blocks_box, unsigned int lod_index, StdVector<Vector3i> *missing_blocks,
			StdVector<Vector3i> *found_blocks_positions, StdVector<VoxelDataBlock> *found_blocks);

	// Decreases the reference count of loaded blocks in the area. Blocks reaching zero will be unloaded.
	// Returns positions where blocks were unloaded, and where they were missing.
	// If `to_save` is not null and some unloaded blocks contained modifications, their data will be returned too.
	// Should only be used if refcounting is used, may fail otherwise.
	void unview_area(Box3i blocks_box, unsigned int lod_index,
			// Blocks that actually got removed (some areas can have no block)
			StdVector<Vector3i> *removed_blocks,
			// Missing blocks are used in case the caller has a collection of loading blocks, so it can cancel them
			StdVector<Vector3i> *missing_blocks,
			// Blocks to save are those that had unsaved modifications
			StdVector<BlockToSave> *to_save);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Metadata queries.
	// Only at LOD0.

	void set_voxel_metadata(Vector3i pos, Variant meta);
	Variant get_voxel_metadata(Vector3i pos);

private:
	void reset_maps_no_settings_lock();

	struct Lod {
		// Storage for edited and cached voxels.
		VoxelDataMap map;

		// Multi-threaded access strategy:
		// - Spatial lock first
		// - Map lock second, while the spatial lock is acquired, just to lookup the map
		// This should be safe assuming the address of hashmap's values remains stable when insertion or removal occurs.
		// If two lods really need to be locked as well, lock the lower index first, and higher index next.

		// Lock protecting the map itself, because it uses a hashmap.
		// This lock should be locked in write mode only when the map gets modified (adding or removing blocks).
		// Otherwise it may be locked in read mode.
		// It is possible to unlock it after we are done querying the map.
		RWLock map_lock;
		// This should be used when reading or writing voxels/metadata in blocks. It uses block coordinates as
		// spatial unit.
		mutable SpatialLock3D spatial_lock;
	};

	static void pre_generate_box(Box3i voxel_box, Span<Lod> lods, unsigned int data_block_size, bool streaming,
			unsigned int lod_count, Ref<VoxelGenerator> generator, VoxelModifierStack &modifiers);

	static inline std::shared_ptr<VoxelBuffer> try_get_voxel_buffer_with_lock(
			const Lod &data_lod, Vector3i block_pos, bool &out_generate) {
		RWLockRead rlock(data_lod.map_lock);
		const VoxelDataBlock *block = data_lod.map.get_block(block_pos);
		if (block == nullptr) {
			// The block is not there, so unless streaming is not enabled, we don't know if it has edits or not.
			return nullptr;
		}

		// The block is there, so we know if it has edits or not.

		// TODO Thread-safety: this checking presence of voxels is not safe.
		// It can change while meshing takes place if a modifier is moved in the same area,
		// because it invalidates cached data (that doesn't require locking the map, and doesn't lock a VoxelBuffer,
		// so there is no sync going on). One way to fix this is to implement a spatial lock.
		if (!block->has_voxels()) {
			out_generate = true;
			return nullptr;
		}
		return block->get_voxels_shared();
	}

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is.
	// LOD 0 is the primary storage for edited data. Higher indices are "mip-maps".
	// A fixed array is used because max lod count is small, and it doesn't require locking by threads.
	// Note that these LODs do not automatically update, it is up to users of the class to trigger it.
	//
	// TODO Optimize: (low priority) this takes more than 5Kb in the object, even when not using LODs.
	// Each LOD contains an RWLock, which is 242 bytes, so *24 it adds up quickly.
	// A solution would be to allocate LODs dynamically in the constructor (the potential presence of LODs doesn't
	// need to change after being constructed, there is no use case for that so far).
	FixedArray<Lod, constants::MAX_LOD> _lods;

	// Area within which voxels can exist.
	// Note, these bounds might not be exactly represented. Volumes are chunk-based, so the result may be
	// approximated to the closest chunk.
	Box3i _bounds_in_voxels;

	uint8_t _lod_count = 1;

	// If enabled, some data blocks can have the "not loaded" and "loaded" status. Which means we can't assume what
	// they contain, until we load them from the stream. If disabled, all edits are loaded in memory, and we know if
	// a block isn't stored, it means we can use the generator and modifiers to obtain its data. This mostly changes
	// how this class is used, streaming itself is not directly implemented in this class.
	bool _streaming_enabled = true;

	// When streaming is disabled, this will tell if all data has finished loading.
	// This is because *everything* will load, we can't tell in advance what is loaded and what isn't by looking at
	// individual blocks.
	bool _full_load_completed = false;

	// Procedural generation stack
	VoxelModifierStack _modifiers;
	Ref<VoxelGenerator> _generator;

	// Persistent storage (file(s)).
	Ref<VoxelStream> _stream;

	// This should be locked when accessing settings members.
	// If other locks are needed simultaneously such as voxel maps, they should always be locked AFTER, to prevent
	// deadlocks.
	//
	// It is not a RWLock because it may be locked for VERY short periods of time (just reading small values).
	// In comparison, RWLock uses a `shared_timed_mutex` under the hood, and locking that for reading locks a
	// mutex internally either way.
	// There are times where locking can take longer, but it only happens rarely, when changing LOD count for
	// example.
	Mutex _settings_mutex;
};

} // namespace zylann::voxel

#endif // VOXEL_DATA_H
