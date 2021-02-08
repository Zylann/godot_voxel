#ifndef VOXEL_INSTANCER_H
#define VOXEL_INSTANCER_H

#include "../../math/rect3i.h"
#include "../../streams/instance_data.h"
#include "../../util/array_slice.h"
#include "../../util/direct_multimesh_instance.h"
#include "../../util/fixed_array.h"
#include "voxel_instance_generator.h"

#include <scene/3d/spatial.h>
//#include <scene/resources/material.h> // Included by node.h lol
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

class VoxelLodTerrain;
class VoxelInstancerRigidBody;
class PhysicsBody;

// Add-on to voxel nodes, allowing to spawn elements on the surface.
// These elements are rendered with hardware instancing, can have collisions, and also be persistent.
class VoxelInstancer : public Spatial {
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

	void set_up_mode(UpMode mode);
	UpMode get_up_mode() const;

	void save_all_modified_blocks();

	// Layers

	int add_layer(int lod_index);

	void set_layer_generator(int layer_index, Ref<VoxelInstanceGenerator> generator);
	void set_layer_persistent(int layer_index, bool persistent);

	void set_layer_mesh(int layer_index, Ref<Mesh> mesh);
	void set_layer_material_override(int layer_index, Ref<Material> material);

	void set_layer_collision_layer(int layer_index, int collision_layer);
	void set_layer_collision_mask(int layer_index, int collision_mask);
	void set_layer_collision_shapes(int layer_index, Array shape_infos);

	void set_layer_from_template(int layer_index, Node *root);

	void remove_layer(int layer_index);

	// Event handlers

	void on_block_data_loaded(Vector3i grid_position, int lod_index,
			std::unique_ptr<VoxelInstanceBlockData> instances);

	void on_block_enter(Vector3i grid_position, int lod_index, Array surface_arrays);
	void on_block_exit(Vector3i grid_position, int lod_index);

	void on_area_edited(Rect3i p_voxel_box);

	void on_body_removed(int block_index, int instance_index);

	// Debug

	int debug_get_block_count() const;

protected:
	void _notification(int p_what);

private:
	struct Block;
	struct Layer;

	void remove_block(int block_index);
	void set_world(World *world);
	void clear_instances();
	void update_visibility();
	void save_block(Vector3i grid_pos, int lod_index) const;

	int find_layer_by_persistent_id(int id) const;
	int generate_persistent_id() const;

	void create_blocks(const VoxelInstanceBlockData *instances_data, Vector3i grid_position, int lod_index,
			Array surface_arrays);

	void create_block_from_transforms(ArraySlice<const Transform> transforms,
			Vector3i grid_position, Layer *layer, unsigned int layer_index, World *world,
			const Transform &block_transform);

	static void _bind_methods();

	struct Block {
		int layer_index;
		Vector3i grid_position;
		DirectMultiMeshInstance multimesh_instance;
		// For physics we use nodes because it's easier to manage.
		// Such instances may be less numerous.
		Vector<VoxelInstancerRigidBody *> bodies;
	};

	struct CollisionShapeInfo {
		Transform transform;
		Ref<Shape> shape;
	};

	struct Layer {
		static const int MAX_ID = 0xffff;

		// This ID identifies the layer uniquely within saved data. Two layers cannot use the same ID.
		// Note: layer indexes are used only for fast access, they are not persistent.
		int persistent_id = -1;
		// If a layer is persistent, any change to its instances will be saved if the volume has a stream
		// supporting instances. It will also not generate on top of modified surfaces.
		// If a layer is not persistent, changes won't get saved, and it will keep generating on all compliant
		// surfaces.
		bool persistent = false;

		int lod_index = 0;

		Ref<VoxelInstanceGenerator> generator;

		// TODO lods?
		Ref<Mesh> mesh;

		// It is preferred to have materials on the mesh already,
		// but this is in case OBJ meshes are used, which often dont have a material of their own
		Ref<Material> material_override;

		int collision_mask = 1;
		int collision_layer = 1;
		Vector<CollisionShapeInfo> collision_shapes;

		HashMap<Vector3i, int, Vector3iHasher> blocks;

		Layer() = default;
		Layer(const Layer &) = delete; // non construction-copyable
		Layer &operator=(const Layer &) = delete; // non copyable
	};

	struct Lod {
		std::vector<int> layers;
		HashMap<Vector3i, bool, Vector3iHasher> modified_blocks;
		// This is a temporary place to store loaded instances data while it's not visible yet.
		// These instances are user-authored ones. If a block does not have an entry there,
		// it will get generated instances.
		// Can't use `HashMap` because it lacks move semantics.
		std::unordered_map<Vector3i, std::unique_ptr<VoxelInstanceBlockData> > loaded_instances_data;

		Lod() = default;
		Lod(const Lod &) = delete; // non construction-copyable
		Lod &operator=(const Lod &) = delete; // non copyable
	};

	UpMode _up_mode = UP_MODE_POSITIVE_Y;

	FixedArray<Lod, MAX_LOD> _lods;
	std::vector<Block *> _blocks; // Does not have nulls
	std::vector<Layer *> _layers; // Can have nulls

	std::vector<Transform> _transform_cache;

	VoxelLodTerrain *_parent;
};

VARIANT_ENUM_CAST(VoxelInstancer::UpMode);

#endif // VOXEL_INSTANCER_H
