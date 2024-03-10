#ifndef VOXEL_GENERATOR_MULTIPASS_CB_H
#define VOXEL_GENERATOR_MULTIPASS_CB_H

#include "../../engine/ids.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/containers/std_vector.h"
#include "../../util/math/box3i.h"
#include "../../util/math/vector2i.h"
#include "../../util/memory/memory.h"
#include "../../util/ref_count.h"
#include "../../util/thread/mutex.h"
#include "../voxel_generator.h"
#include "voxel_generator_multipass_cb_structs.h"
#include "voxel_tool_multipass_generator.h" // Must be included so we can define GDVIRTUAL methods

#if defined(ZN_GODOT)
#include <core/object/script_language.h> // needed for GDVIRTUAL macro
#include <core/object/gdvirtual.gen.inc> // Also needed for GDVIRTUAL macro...
#endif

namespace zylann {
namespace voxel {

// TODO Prevent shared usage on more than one terrain, or find a way to support it
// TODO Prevent usage on VoxelLodTerrain, or make it return empty blocks

// Generator runing multiple inter-dependent passes.
// It is useful to generates small and medium-sized structures on terrain such as trees.
// Contrary to most other generators, it holds a state (cache) requiring it to be aware of terrain viewers.
// It is also internally column-based (CB), so it can work on vertical stacks of blocks instead of single blocks at a
// time.
class VoxelGeneratorMultipassCB : public VoxelGenerator {
	GDCLASS(VoxelGeneratorMultipassCB, VoxelGenerator)
public:
	static constexpr int MAX_PASSES = VoxelGeneratorMultipassCBStructs::MAX_PASSES;
	static constexpr int MAX_PASS_EXTENT = VoxelGeneratorMultipassCBStructs::MAX_PASS_EXTENT;

	static constexpr int MAX_SUBPASSES = VoxelGeneratorMultipassCBStructs::MAX_SUBPASSES;
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

	VoxelGeneratorMultipassCB();
	~VoxelGeneratorMultipassCB();

	bool supports_lod() const override {
		return false;
	}

	Result generate_block(VoxelQueryData &input) override;
	int get_used_channels_mask() const override;

	IThreadedTask *create_block_task(const VoxelGenerator::BlockTaskParams &params) const override;

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
	virtual void generate_pass(VoxelGeneratorMultipassCBStructs::PassInput input);

	int get_pass_count() const;
	void set_pass_count(int pass_count);

	int get_column_base_y_blocks() const;
	void set_column_base_y_blocks(int new_y);

	int get_column_height_blocks() const;
	void set_column_height_blocks(int new_height);

	int get_pass_extent_blocks(int pass_index) const;
	void set_pass_extent_blocks(int pass_index, int new_extent);

	// Run the generator to get a particular column from scratch, using a single thread for better script debugging
	// (since Godot 4 still doesn't support debugging scripts in different threads, at time of writing). This doesn't
	// use the internal cache and can be extremely slow.
	TypedArray<godot::VoxelBuffer> debug_generate_test_column(Vector2i column_position_blocks);

	// Internal

	std::shared_ptr<VoxelGeneratorMultipassCBStructs::Internal> get_internal() const;

	// Must be called when a viewer gets paired, moved, or unpaired from the terrain.
	// Pairing should send an empty previous box.
	// Moving should send the the previous box and new box.
	// Unpairing should send an empty box as the current box.
	void process_viewer_diff(ViewerID viewer_id, Box3i p_requested_box, Box3i p_prev_requested_box) override;

	void clear_cache() override;

	// Editor

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

	struct DebugColumnState {
		Vector2i position;
		int8_t subpass_index;
		uint8_t viewer_count;
	};

	bool debug_try_get_column_states(StdVector<DebugColumnState> &out_states);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

// TODO GDX: Defining custom virtual functions is not supported...
#if defined(ZN_GODOT)
	GDVIRTUAL2(_generate_pass, Ref<VoxelToolMultipassGenerator>, int)

	// Called outside of the column region so it is possible to define what generates past the top and bottom
	GDVIRTUAL2(_generate_block_fallback, Ref<godot::VoxelBuffer>, Vector3i)

	GDVIRTUAL0RC(int, _get_used_channels_mask)
#endif

private:
	void process_viewer_diff_internal(Box3i p_requested_box, Box3i p_prev_requested_box);
	void re_initialize_column_refcounts();
	void generate_block_fallback_script(VoxelQueryData &input);

	// This must be called each time the structure of passes changes (number of passes, extents)
	template <typename F>
	void reset_internal(F f) {
		std::shared_ptr<VoxelGeneratorMultipassCBStructs::Internal> old_internal = get_internal();

		std::shared_ptr<VoxelGeneratorMultipassCBStructs::Internal> new_internal =
				make_shared_instance<VoxelGeneratorMultipassCBStructs::Internal>();

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
	StdVector<PairedViewer> _paired_viewers;

	// Threads can be very busy working on this data structure. Yet in the editor, users can modify its parameters
	// anytime, which could break everything. So when any parameter changes, a copy of this structure is made, and the
	// old one is de-referenced from here (and will be thrown away once the last thread is done with it). This incurs
	// quite some overhead when setting properties, however this should mostly happen in editor and resource loading,
	// never in the middle of gameplay.
	std::shared_ptr<VoxelGeneratorMultipassCBStructs::Internal> _internal;
	Mutex _internal_mutex;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATOR_MULTIPASS_CB_H
