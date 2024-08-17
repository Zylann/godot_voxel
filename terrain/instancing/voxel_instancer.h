#ifndef VOXEL_INSTANCER_H
#define VOXEL_INSTANCER_H

#include "../../constants/voxel_constants.h"
#include "../../streams/instance_data.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/containers/std_unordered_set.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/node_3d.h"
#include "../../util/godot/direct_multimesh_instance.h"
#include "../../util/math/box3i.h"
#include "../../util/memory/memory.h"
#include "instance_library_item_listener.h"
#include "up_mode.h"

#ifdef TOOLS_ENABLED
#include "../../util/godot/core/version.h"
#include "../../util/godot/debug_renderer.h"
#endif

#include <limits>

ZN_GODOT_FORWARD_DECLARE(class PhysicsBody3D);

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelNode;
class VoxelInstancerRigidBody;
class VoxelInstanceComponent;
class VoxelInstanceLibrary;
class VoxelInstanceLibraryItem;
class VoxelInstanceLibrarySceneItem;
class VoxelTool;
class SaveBlockDataTask;
class BufferedTaskScheduler;
struct InstanceBlockData;
struct InstancerQuickReloadingCache;
struct InstancerTaskOutputQueue;
struct InstanceLibraryMultiMeshItemSettings;

// Note: a large part of this node could be made generic to support the sole idea of instancing within octants?
// Even nodes like gridmaps could be rebuilt on top of this, if its concept of "grid" was decoupled.
// It is coupled to terrain at the moment because of performance.

// Add-on to voxel nodes, allowing to spawn elements on the surface.
// These elements are rendered with hardware instancing, can have collisions, and also be persistent.
class VoxelInstancer : public Node3D, public IInstanceLibraryItemListener {
	GDCLASS(VoxelInstancer, Node3D)
public:
	static const int MAX_LOD = 8;

	// I didn't want this enum to be here on the C++ side, because it prevents forward-declaring the class it is in.
	// However Godot is forcing me to.
	// `VARIANT_ENUM_CAST(ns1::ns2::Enum)` assumes the enum is in a class, so it generates its name as being `ns2.Enum`,
	// which confuses docs and GDExtension dumps. There doesn't seem to be a way to register that enum as global either.
	using UpMode = zylann::voxel::UpMode;

	VoxelInstancer();
	~VoxelInstancer();

	// Properties

	void set_up_mode(UpMode mode);
	UpMode get_up_mode() const;

	void set_library(Ref<VoxelInstanceLibrary> library);
	Ref<VoxelInstanceLibrary> get_library() const;

	// Actions

	void save_all_modified_blocks(
			BufferedTaskScheduler &tasks,
			std::shared_ptr<AsyncDependencyTracker> tracker,
			bool with_flush
	);

	// Event handlers

	// void on_data_block_loaded(Vector3i grid_position, unsigned int lod_index, UniquePtr<InstanceBlockData>
	// instances);
	void on_mesh_block_enter(Vector3i render_grid_position, unsigned int lod_index, Array surface_arrays);
	void on_mesh_block_exit(Vector3i render_grid_position, unsigned int lod_index);
	void on_area_edited(Box3i p_voxel_box);
	void on_body_removed(Vector3i data_block_position, unsigned int render_block_index, unsigned int instance_index);
	void on_scene_instance_removed(
			Vector3i data_block_position,
			unsigned int render_block_index,
			unsigned int instance_index
	);
	void on_scene_instance_modified(Vector3i data_block_position, unsigned int render_block_index);
	void on_data_block_saved(Vector3i data_grid_position, unsigned int lod_index);

	// Internal properties

	void set_mesh_block_size_po2(unsigned int p_mesh_block_size_po2);
	void set_data_block_size_po2(unsigned int p_data_block_size_po2);
	void update_mesh_lod_distances_from_parent();

	int get_library_item_id_from_render_block_index(unsigned render_block_index) const;

	// Debug

	int debug_get_block_count() const;
	void debug_get_instance_counts(StdUnorderedMap<uint32_t, uint32_t> &counts_per_layer) const;
	void debug_dump_as_scene(String fpath) const;
	Node *debug_dump_as_nodes() const;

	void debug_set_draw_enabled(bool enabled);
	bool debug_is_draw_enabled() const;

	enum DebugDrawFlag { //
		DEBUG_DRAW_ALL_BLOCKS,
		DEBUG_DRAW_EDITED_BLOCKS,
		DEBUG_DRAW_FLAGS_COUNT
	};

	void debug_set_draw_flag(DebugDrawFlag flag_index, bool enabled);
	bool debug_get_draw_flag(DebugDrawFlag flag_index) const;

	// Editor

#ifdef TOOLS_ENABLED
#if defined(ZN_GODOT)
	PackedStringArray get_configuration_warnings() const override;
#elif defined(ZN_GODOT_EXTENSION)
	PackedStringArray _get_configuration_warnings() const override;
#endif
	virtual void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

protected:
	void _notification(int p_what);

private:
	struct Layer;

	void process();
	void process_task_results();
	void process_mesh_lods();

	void add_layer(int layer_id, int lod_index);
	void remove_layer(int layer_id);
	unsigned int create_block(Layer &layer, uint16_t layer_id, Vector3i grid_position, bool pending_instances);
	void remove_block(unsigned int block_index);
	void set_world(World3D *world);
	void clear_blocks();
	void clear_blocks_in_layer(int layer_id);
	void clear_layers();
	void update_visibility();
	SaveBlockDataTask *save_block(
			Vector3i data_grid_pos,
			int lod_index,
			std::shared_ptr<AsyncDependencyTracker> tracker,
			bool with_flush,
			bool cache_while_saving
	);

	// Get a layer assuming it exists
	Layer &get_layer(int id);
	const Layer &get_layer_const(int id) const;

	void regenerate_layer(uint16_t layer_id, bool regenerate_blocks);
	void update_layer_meshes(int layer_id);
	void update_layer_scenes(int layer_id);
	void create_render_blocks(Vector3i grid_position, int lod_index, Array surface_arrays);

#ifdef TOOLS_ENABLED
	void process_gizmos();
#endif

	struct SceneInstance {
		// Owned by the scene tree.
		VoxelInstanceComponent *component = nullptr;
		Node3D *root = nullptr;
	};

	SceneInstance create_scene_instance(
			const VoxelInstanceLibrarySceneItem &scene_item,
			int instance_index,
			unsigned int block_index,
			Transform3D transform,
			int data_block_size_po2
	);

	void update_block_from_transforms(
			int block_index,
			Span<const Transform3f> transforms,
			Vector3i grid_position,
			Layer &layer,
			const VoxelInstanceLibraryItem &item_base,
			uint16_t layer_id,
			World3D &world,
			const Transform3D &block_transform,
			Vector3 block_local_position
	);

	void on_library_item_changed(int item_id, IInstanceLibraryItemListener::ChangeType change) override;

	struct Block;

	static void remove_floating_multimesh_instances(
			Block &block,
			const Transform3D &parent_transform,
			Box3i p_voxel_box,
			const VoxelTool &voxel_tool,
			int block_size_po2
	);

	static void remove_floating_scene_instances(
			Block &block,
			const Transform3D &parent_transform,
			Box3i p_voxel_box,
			const VoxelTool &voxel_tool,
			int block_size_po2
	);

	static void update_mesh_from_mesh_lod(
			Block &block,
			const InstanceLibraryMultiMeshItemSettings &settings,
			bool hide_beyond_max_lod,
			bool instancer_is_visible
	);

	Dictionary _b_debug_get_instance_counts() const;

	static void _bind_methods();

	// TODO Rename RenderBlock?
	struct Block {
		uint16_t layer_id = 0;
		// Distance-based LOD index.
		// Can be one index higher than max mesh lod count in case it should hide beyond last LOD
		uint8_t current_mesh_lod = 0;
		// LOD index corresponding to the terrain's ground chunk system
		uint8_t lod_index = 0;
		// If true, the block is waiting to be populated asynchronously. We create blocks in this state so when async
		// generation completes, we can check if the block is still present.
		// TODO Unused?
		bool pending_instances = false;
		// Position in mesh block coordinate system
		Vector3i grid_position;
		zylann::godot::DirectMultiMeshInstance multimesh_instance;
		// For physics we use nodes because it's easier to manage.
		// Such instances may be less numerous.
		// If the item associated to this block has no collisions, this will be empty.
		// Indices in the vector correspond to index of the instance in multimesh.
		StdVector<VoxelInstancerRigidBody *> bodies;
		StdVector<SceneInstance> scene_instances;
	};

	struct Layer {
		unsigned int lod_index;
		// Blocks indexed by grid position.
		// Keys follow the mesh block coordinate system.
		StdUnorderedMap<Vector3i, unsigned int> blocks;
	};

	struct MeshLodDistances {
		// Multimesh LOD updates based on the distance between the camera and the center of the block.
		// Two distances are used to implement hysteresis, which allows to avoid oscillating too fast between lods.

		// TODO Need to investigate if Godot 4 implements LOD for multimeshes
		// Despite this, due to how Godot 4 implements LOD, it may still be beneficial to have a custom LOD system,
		// so we can switch to impostors rather than only decimating geometry

		// Distance above which the mesh starts being used, taking precedence over meshes of lower distance.
		float enter_distance_squared;
		// Distance under which the mesh stops being used
		float exit_distance_squared;
	};

	struct Lod : public NonCopyable {
		// Unordered list of layer IDs using this LOD level.
		StdVector<int> layers;

		// Blocks that have have unsaved changes.
		// Keys follows the data block coordinate system.
		StdUnorderedSet<Vector3i> modified_blocks;

		// This is a temporary place to store loaded instances data while it's not visible yet.
		// These instances are user-authored ones. If a block does not have an entry there,
		// it will get generated instances.
		// Keys follows the data block coordinate system.
		// Can't use Godot's `HashMap` because it lacks move semantics.
		// StdUnorderedMap<Vector3i, UniquePtr<InstanceBlockData>> loaded_instances_data;

		// Blocks that contain edited data (not generated).
		// Keys follows the data block coordinate system.
		StdUnorderedSet<Vector3i> edited_data_blocks;

		std::shared_ptr<InstancerQuickReloadingCache> quick_reload_cache;

		// FixedArray<MeshLodDistances, VoxelInstanceLibraryMultiMeshItem::MAX_MESH_LODS> mesh_lod_distances;
	};

	UpMode _up_mode = UP_MODE_POSITIVE_Y;

	FixedArray<Lod, MAX_LOD> _lods;

	// Does not have nulls. Indices matter.
	StdVector<UniquePtr<Block>> _blocks;

	// Each layer corresponds to a library item. Addresses of values in the map are expected to be stable.
	StdUnorderedMap<int, Layer> _layers;

	Ref<VoxelInstanceLibrary> _library;

	VoxelNode *_parent = nullptr;
	unsigned int _parent_data_block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;
	unsigned int _parent_mesh_block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;
	FixedArray<float, MAX_LOD> _mesh_lod_distances;
	// Vector3 _mesh_lod_last_update_camera_position;
	// float _mesh_lod_update_camera_threshold_distance = 8.f;
	unsigned int _mesh_lod_time_sliced_block_index = 0;

	std::shared_ptr<InstancerTaskOutputQueue> _loading_results;

#ifdef TOOLS_ENABLED
	zylann::godot::DebugRenderer _debug_renderer;
	bool _gizmos_enabled = false;
	uint8_t _debug_draw_flags = 0;
#endif
};

} // namespace voxel
} // namespace zylann

VARIANT_ENUM_CAST(zylann::voxel::VoxelInstancer::UpMode);
VARIANT_ENUM_CAST(zylann::voxel::VoxelInstancer::DebugDrawFlag);

#endif // VOXEL_INSTANCER_H
