#ifndef VOXEL_GENERATOR_MULTIPASS_CB_H
#define VOXEL_GENERATOR_MULTIPASS_CB_H

#include "../../engine/ids.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../util/ref_count.h"
#include "../../util/small_vector.h"
#include "../../util/thread/spatial_lock_2d.h"
#include "../voxel_generator.h"

namespace zylann {

class IThreadedTask;

namespace voxel {

// TODO Prevent shared usage on more than one terrain, or find a way to support it
// TODO Prevent usage on VoxelLodTerrain, or make it return empty blocks

class VoxelGeneratorMultipassCB : public VoxelGenerator {
	GDCLASS(VoxelGeneratorMultipassCB, VoxelGenerator)
public:
	// Pass limit is pretty low because in practice not that many should be needed, and it gets expensive really quick
	static const int MAX_PASSES = 4;
	static const int MAX_PASS_EXTENT = 2;

	// Passes are internally split into subpasses: 1 subpass for pass 0, 2 subpasses for the others. Each subpass
	// depends on the previous. This is to cover side-effect of passes that can modify neighbors.
	static const int MAX_SUBPASSES = MAX_PASSES * 2 - 1; // get_subpass_count_from_pass_count(MAX_PASSES);
	// I would have gladly used a constexpr function but apparently if it's a static method it doesn't work unless it is
	// moved outside of the class, which is inconvenient

	// Sane safety maximum.
	// For reference, Minecraft is 24 blocks high (384 voxels)
	static constexpr int MAX_COLUMN_HEIGHT_BLOCKS = 32;

	static inline int get_subpass_count_from_pass_count(int pass_count) {
		return pass_count * 2 - 1;
	}

	static inline int get_pass_index_from_subpass(int subpass_index) {
		return (subpass_index + 1) / 2;
	}

	struct Block {
		VoxelBufferInternal voxels;

		// If set, this task will be scheduled when the block's generation is complete.
		// Only a non-scheduled, non-running task can be referenced here.
		// These tasks also must NOT have spawned subtasks. That is, they are only owned by the block while they are
		// here.
		// In other words, we use this pointer to prevent asking columns to generate multiple times, because there can
		// be multiple block requests for one column.
		IThreadedTask *final_pending_task = nullptr;

		Block() {}

		Block(Block &&other) {
			voxels = std::move(other.voxels);
			final_pending_task = other.final_pending_task;
		}

		Block &operator=(Block &&other) {
			voxels = std::move(other.voxels);
			final_pending_task = other.final_pending_task;
			return *this;
		}

		~Block() {
			ZN_ASSERT_RETURN_MSG(final_pending_task == nullptr, "Unhandled task leaked!");
		}
	};

	struct Column {
		RefCount viewers;
		// Index of the last subpass that was executed directly on this chunk.
		// -1 means the chunk just got created and no subpass has run on it yet.
		int8_t subpass_index = -1;
		bool saving = false;
		bool loading = false;

		// Each bit is set to 1 when a task is pending to process this block at a given subpass.
		uint8_t pending_subpass_tasks_mask = 0;

		// Currently unused, because if chunks get removed from the cache or don't get saved for any reason,
		// it can become out of sync and we wouldn't know. It would be a nice optimization tho...
		//
		// Caches how many chunks in the neighborhood have been processed while having access to this one (included).
		// This is an optimization allowing to reduce chunk map queries. This is an array because it is possible
		// for a subpass to start writing into a block that completed its previous subpass but hasn't started the next
		// subpass.
		FixedArray<uint8_t, MAX_SUBPASSES> subpass_iterations;

		// Vertical stack of blocks. Must never be resized, except when loaded or unloaded.
		// TODO Maybe replace with a dynamic non-resizeable array?
		std::vector<Block> blocks;

		Column() {
			fill(subpass_iterations, uint8_t(0));
		}
	};

	struct Map {
		std::unordered_map<Vector2i, Column> columns;
		// Protects the hashmap itself
		Mutex mutex;
		// Protects columns
		mutable SpatialLock2D spatial_lock;

		~Map();
	};

	struct Pass {
		// How many blocks to load using the previous pass around the generated area, so that neighbors can be accessed
		// in case the pass has effects across blocks.
		// Constraints: the first pass cannot have dependencies; other passes must have dependencies.
		int8_t dependency_extents = 0;
	};

	// Internal state of the generator.
	struct Internal {
		// Map used solely for generation purposes. It acts like a cache so we don't recompute the same passes many
		// times. In theory we could generate blocks without caching or streaming anything, but that would mean every
		// block request would have to repeatedly generate so much data around it (column, neighbors, neighbors of
		// neighbors...), it would be prohibitively slow.
		// As a cache, it can be deleted anytime (taking care of thread safety ofc).
		Map map;

		// Params: they should never change. Changing them involves making a whole new instance.
		SmallVector<Pass, MAX_PASSES> passes;
		int column_base_y_blocks = -4;
		int column_height_blocks = 8;

		// Set to `true` if the generator's configuration changed. Means a new instance of Internal has been made.
		// Existing tasks may still finish their work using the old instance, but results will be thrown away. Such
		// tasks can end faster if they check this boolean.
		bool expired = false;

		void copy_params(const Internal &other) {
			passes = other.passes;
			column_base_y_blocks = other.column_base_y_blocks;
			column_height_blocks = other.column_height_blocks;
		}
	};

	VoxelGeneratorMultipassCB();
	~VoxelGeneratorMultipassCB();

	Result generate_block(VoxelQueryData &input) override;

	struct PassInput {
		Span<Block *> grid;
		Vector3i grid_size;
		// Position of the grid's lower corner.
		Vector3i grid_origin;
		// Position of the main block or column in world coordinates.
		Vector3i main_block_position;
		int block_size = 0;
		int pass_index = 0;
	};

	// Executes a pass on a grid of blocks.
	// The grid contains a central column of blocks, where the main processing must happen.
	// It is surrounded by neighbors, which are accessible in case main processing affects them partially (structures
	// intersecting, light spreading out...). "Main processing" will be invoked only once per column and per pass.
	// Each block in the grid is guaranteed to have been processed *at least* by preceding passes.
	// However, neighbor blocks can also have been processed by the current pass already ("main" or not), which is a
	// side-effect of being able to modify neighbors. This is where you need to be careful with seeded generation: if
	// two neighbor blocks both generate things that overlap each other, one will necessarily generate before the other.
	// That order is unpredictable because of multithreading and player movement. So it's up to you to ensure each
	// order produces the same outcome. If you don't, it might not be a big issue, but generating the same world twice
	// will therefore have some differences.
	virtual void generate_pass(PassInput input);

	int get_pass_count() const;
	void set_pass_count(int pass_count);

	int get_column_base_y_blocks() const;
	void set_column_base_y_blocks(int new_y);

	int get_column_height_blocks() const;
	void set_column_height_blocks(int new_height);

	int get_pass_extent_blocks(int pass_index) const;
	void set_pass_extent_blocks(int pass_index, int new_extent);

	// Internal

	std::shared_ptr<Internal> get_internal() const;

	// Must be called when a viewer gets paired, moved, or unpaired from the terrain.
	// Pairing should send an empty previous box.
	// Moving should send the the previous box and new box.
	// Unpairing should send an empty box as the current box.
	void process_viewer_diff(ViewerID viewer_id, Box3i p_requested_box, Box3i p_prev_requested_box);

	void clear_cache();

	struct DebugColumnState {
		Vector2i position;
		int8_t subpass_index;
		uint8_t viewer_count;
	};

	bool debug_try_get_column_states(std::vector<DebugColumnState> &out_states);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

private:
	void process_viewer_diff_internal(Box3i p_requested_box, Box3i p_prev_requested_box);
	void re_initialize_column_refcounts();

	// This must be called each time the structure of passes changes (number of passes, extents)
	template <typename F>
	void reset_internal(F f) {
		std::shared_ptr<Internal> old_internal = get_internal();

		std::shared_ptr<VoxelGeneratorMultipassCB::Internal> new_internal =
				make_shared_instance<VoxelGeneratorMultipassCB::Internal>();

		new_internal->copy_params(*old_internal);

		f(*new_internal);

		{
			MutexLock mlock(_internal_mutex);
			_internal = new_internal;
			old_internal->expired = true;
		}

		// Note, resetting the cache also means we lost all viewer refcounts in columns, which could be different now.
		// For example if pass count or extents have changed, viewers will need to reference a larger area.
		// Depending on the context, we may call `re_initialize_column_refcounts()`.

		// Alternatives:
		// - Ticket counter. Lock whole map (or some shared mutex that tasks also lock when they run) and increment,
		// then any ongoing task is forbidden to modify anything in the map if tickets don't match.

		// Issue:
		// This kind of generator is very likely to be used with a script. But as with the classic VoxelGeneratorScript,
		// it is totally unsafe to modify the script while it executes in threads in the editor...
	}

	static void _bind_methods();

	struct PairedViewer {
		ViewerID id;
		Box3i request_box;
	};

	// Keeps track of viewers to workaround some edge cases.
	// This is very frustrating, because it is tied to VoxelTerrain and it relates to use cases we usually never
	// encounter during gameplay. For example if a user changes the number of passes in the editor while terrain is
	// loaded, the ref-counted area to load for viewers in the cache has to become larger, and we can't make this happen
	// without having access to the list of paired viewers... if we don't, and if the user doesn't re-generate the
	// terrain, moving around will start causing failed generation requests in a loop because the cache of
	// partially-generated columns won't be in the right state...
	std::vector<PairedViewer> _paired_viewers;

	// Threads can be very busy working on this data structure. Yet in the editor, users can modify its parameters
	// anytime, which could break everything. So when any parameter changes, a copy of this structure is made, and the
	// old one is de-referenced from here (and will be thrown away once the last thread is done with it). This incurs
	// quite some overhead when setting properties, however this should mostly happen in editor and resource loading,
	// never in the middle of gameplay.
	std::shared_ptr<Internal> _internal;
	Mutex _internal_mutex;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATOR_MULTIPASS_CB_H
