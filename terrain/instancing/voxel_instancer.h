#ifndef VOXEL_INSTANCER_H
#define VOXEL_INSTANCER_H

#include "../../streams/instance_data.h"
#include "../../util/fixed_array.h"
#include "../../util/godot/direct_multimesh_instance.h"
#include "../../util/math/box3i.h"
#include "voxel_instance_generator.h"
#include "voxel_instance_library.h"
#include "voxel_instance_library_item.h"

#include <scene/3d/spatial.h>
//#include <scene/resources/material.h> // Included by node.h lol
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

class VoxelLodTerrain;
class VoxelInstancerRigidBody;
class VoxelInstanceComponent;
class VoxelInstanceLibrarySceneItem;
class VoxelTool;
class PhysicsBody;

// Note: a large part of this node could be made generic to support the sole idea of instancing within octants?
// Even nodes like gridmaps could be rebuilt on top of this, if its concept of "grid" was decoupled.

// Add-on to voxel nodes, allowing to spawn elements on the surface.
// These elements are rendered with hardware instancing, can have collisions, and also be persistent.
class VoxelInstancer : public Spatial, public VoxelInstanceLibrary::IListener {
	GDCLASS(VoxelInstancer, Spatial)
public:
	static const int MAX_LOD = 8;

	enum UpMode {
		UP_MODE_POSITIVE_Y = VoxelInstanceGenerator::UP_MODE_POSITIVE_Y,
		UP_MODE_SPHERE = VoxelInstanceGenerator::UP_MODE_SPHERE,
		UP_MODE_COUNT = VoxelInstanceGenerator::UP_MODE_COUNT
	};

	VoxelInstancer();
	~VoxelInstancer();

	// Properties

	void set_up_mode(UpMode mode);
	UpMode get_up_mode() const;

	void set_library(Ref<VoxelInstanceLibrary> library);
	Ref<VoxelInstanceLibrary> get_library() const;

	// Actions

	void save_all_modified_blocks();

	// Event handlers

	void on_data_block_loaded(Vector3i grid_position, unsigned int lod_index,
			std::unique_ptr<VoxelInstanceBlockData> instances);
	void on_mesh_block_enter(Vector3i render_grid_position, unsigned int lod_index, Array surface_arrays);
	void on_mesh_block_exit(Vector3i render_grid_position, unsigned int lod_index);
	void on_area_edited(Box3i p_voxel_box);
	void on_body_removed(Vector3i data_block_position, unsigned int render_block_index, int instance_index);
	void on_scene_instance_removed(Vector3i data_block_position, unsigned int render_block_index, int instance_index);
	void on_scene_instance_modified(Vector3i data_block_position, unsigned int render_block_index);

	// Debug

	int debug_get_block_count() const;
	Dictionary debug_get_instance_counts() const;

	String get_configuration_warning() const override;

protected:
	void _notification(int p_what);

private:
	struct Block;
	struct Layer;

	void process_mesh_lods();

	void add_layer(int layer_id, int lod_index);
	void remove_layer(int layer_id);
	int create_block(Layer *layer, uint16_t layer_id, Vector3i grid_position);
	void remove_block(unsigned int block_index);
	void set_world(World *world);
	void clear_blocks();
	void clear_blocks_in_layer(int layer_id);
	void clear_layers();
	void update_visibility();
	void save_block(Vector3i data_grid_pos, int lod_index) const;
	Layer *get_layer(int id);
	const Layer *get_layer_const(int id) const;
	void regenerate_layer(uint16_t layer_id, bool regenerate_blocks);
	void update_layer_meshes(int layer_id);
	void update_layer_scenes(int layer_id);
	void create_render_blocks(Vector3i grid_position, int lod_index, Array surface_arrays);

	struct SceneInstance {
		VoxelInstanceComponent *component = nullptr;
		Spatial *root = nullptr;
	};

	SceneInstance create_scene_instance(const VoxelInstanceLibrarySceneItem &scene_item,
			int instance_index, unsigned int block_index, Transform transform, int data_block_size_po2);

	void update_block_from_transforms(int block_index, Span<const Transform> transforms,
			Vector3i grid_position, Layer *layer, const VoxelInstanceLibraryItemBase *item_base, uint16_t layer_id,
			World *world, const Transform &block_transform);

	void on_library_item_changed(int item_id, VoxelInstanceLibraryItem::ChangeType change) override;

	struct Block;

	static void remove_floating_multimesh_instances(Block &block, const Transform &parent_transform, Box3i p_voxel_box,
			const VoxelTool &voxel_tool, int block_size_po2);

	static void remove_floating_scene_instances(Block &block, const Transform &parent_transform, Box3i p_voxel_box,
			const VoxelTool &voxel_tool, int block_size_po2);

	static void _bind_methods();

	// TODO Rename RenderBlock?
	struct Block {
		uint16_t layer_id;
		uint8_t current_mesh_lod = 0;
		uint8_t lod_index;
		// Position in mesh block coordinate system
		Vector3i grid_position;
		DirectMultiMeshInstance multimesh_instance;
		// For physics we use nodes because it's easier to manage.
		// Such instances may be less numerous.
		// If the item associated to this block has no collisions, this will be empty.
		// Indices in the vector correspond to index of the instance in multimesh.
		Vector<VoxelInstancerRigidBody *> bodies;
		Vector<SceneInstance> scene_instances;
	};

	struct Layer {
		unsigned int lod_index;
		// Blocks indexed by grid position.
		// Keys follow the mesh block coordinate system.
		HashMap<Vector3i, unsigned int, Vector3iHasher> blocks;
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

	struct Lod {
		std::vector<int> layers;

		// Blocks that have have unsaved changes.
		// Keys follows the data block coordinate system.
		HashMap<Vector3i, bool, Vector3iHasher> modified_blocks;

		// This is a temporary place to store loaded instances data while it's not visible yet.
		// These instances are user-authored ones. If a block does not have an entry there,
		// it will get generated instances.
		// Keys follows the data block coordinate system.
		// Can't use `HashMap` because it lacks move semantics.
		std::unordered_map<Vector3i, std::unique_ptr<VoxelInstanceBlockData>> loaded_instances_data;

		FixedArray<MeshLodDistances, VoxelInstanceLibraryItem::MAX_MESH_LODS> mesh_lod_distances;

		Lod() = default;
		Lod(const Lod &) = delete; // non construction-copyable
		Lod &operator=(const Lod &) = delete; // non copyable
	};

	UpMode _up_mode = UP_MODE_POSITIVE_Y;

	FixedArray<Lod, MAX_LOD> _lods;
	std::vector<Block *> _blocks; // Does not have nulls
	HashMap<int, Layer> _layers; // Each layer corresponds to a library item
	Ref<VoxelInstanceLibrary> _library;

	std::vector<Transform> _transform_cache;

	VoxelLodTerrain *_parent;
};

VARIANT_ENUM_CAST(VoxelInstancer::UpMode);

#endif // VOXEL_INSTANCER_H
