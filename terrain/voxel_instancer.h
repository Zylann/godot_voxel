#ifndef VOXEL_INSTANCER_H
#define VOXEL_INSTANCER_H

#include "../math/rect3i.h"
#include "../streams/instance_data.h"
#include "../util/array_slice.h"
#include "../util/direct_multimesh_instance.h"
#include "../util/fixed_array.h"

#include <scene/3d/spatial.h>
//#include <scene/resources/material.h> // Included by node.h lol
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

class VoxelGenerator;
class VoxelLodTerrain;
class VoxelInstancerRigidBody;
class PhysicsBody;

// TODO Decouple this?
// class VoxelInstanceGenerator : public Resource {
// 	GDCLASS(VoxelInstanceGenerator, Resource)
// public:
// 	void generate_transforms(std::vector<Transform> &out_transforms,
// 			Vector3i grid_position, int lod_index, Ref<VoxelBuffer> voxels, Array surface_arrays);
// };

// Add-on to voxel nodes, allowing to spawn elements on the surface.
// These elements are rendered with hardware instancing, can have collisions, and also be persistent.
class VoxelInstancer : public Spatial {
	GDCLASS(VoxelInstancer, Spatial)
public:
	static const int MAX_LOD = 8;

	VoxelInstancer();
	~VoxelInstancer();

	// enum Source {
	// 	SOURCE_VERTICES
	// 	SOURCE_FACES
	// };

	// Tells how to interpret where "upwards" is in the current volume
	enum UpMode {
		// The world is a plane, so altitude is obtained from the Y coordinate and upwards is always toward +Y.
		UP_MODE_POSITIVE_Y,
		// The world is a sphere (planet), so altitude is obtained from distance to the origin (0,0,0),
		// and upwards is the normalized vector from origin to current position.
		UP_MODE_SPHERE,
		// How many up modes there are
		UP_MODE_COUNT
	};

	void set_up_mode(UpMode mode);
	UpMode get_up_mode() const;

	void save_all_modified_blocks();

	// Layers

	int add_layer(int lod_index);

	void set_layer_mesh(int layer_index, Ref<Mesh> mesh);
	void set_layer_material_override(int layer_index, Ref<Material> material);

	void set_layer_random_vertical_flip(int layer_index, bool flip_enabled);
	void set_layer_density(int layer_index, float density);
	void set_layer_min_scale(int layer_index, float min_scale);
	void set_layer_max_scale(int layer_index, float max_scale);
	void set_layer_vertical_alignment(int layer_index, float vertical_alignment);
	void set_layer_offset_along_normal(int layer_index, float offset);
	void set_layer_min_slope_degrees(int layer_index, float degrees);
	void set_layer_max_slope_degrees(int layer_index, float degrees);
	void set_layer_min_height(int layer_index, float h);
	void set_layer_max_height(int layer_index, float h);

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
	void generate_block_on_each_layer(Vector3i grid_pos, int lod_index, Array surface_arrays);

	int find_layer_by_id(int id) const;

	void create_loaded_blocks(const VoxelInstanceBlockData &instances_data, Vector3i grid_position, int lod_index);

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

		// This ID identifies the layer uniquely within the current volume, and within saved data.
		// Note: layer indexes are used only for fast access, they are not persistent.
		int id = -1;
		// TODO Allow non-persistent layers?
		// These won't need an ID, will not be saved, and will always re-generate. Also rename `id` to `persistent_id`.

		int lod_index = 0;

		float density = 0.1f;
		float vertical_alignment = 1.f;
		float min_scale = 1.f;
		float max_scale = 1.f;
		float offset_along_normal = 0.f;
		float min_surface_normal_y = -1.f;
		float max_surface_normal_y = 1.f;
		float min_height = std::numeric_limits<float>::min();
		float max_height = std::numeric_limits<float>::max();
		bool random_vertical_flip = false;

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
