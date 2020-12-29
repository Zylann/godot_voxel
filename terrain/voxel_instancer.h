#ifndef VOXEL_INSTANCER_H
#define VOXEL_INSTANCER_H

#include "../math/vector3i.h"
#include "../util/direct_multimesh_instance.h"
#include "../util/fixed_array.h"

#include <scene/3d/spatial.h>
#include <vector>

class VoxelGenerator;
class VoxelLodTerrain;

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

	enum UpMode {
		UP_MODE_POSITIVE_Y,
		UP_MODE_SPHERE,
		UP_MODE_COUNT
	};

	void set_up_mode(UpMode mode);
	UpMode get_up_mode() const;

	int add_layer(int lod_index);
	void set_layer_mesh(int layer_index, Ref<Mesh> mesh);
	void set_layer_random_vertical_flip(int layer_index, bool flip_enabled);
	void set_layer_density(int layer_index, float density);
	void set_layer_min_scale(int layer_index, float min_scale);
	void set_layer_max_scale(int layer_index, float max_scale);
	void set_layer_vertical_alignment(int layer_index, float vertical_alignment);
	void set_layer_offset_along_normal(int layer_index, float offset);
	void remove_layer(int layer_index);

	void on_block_enter(Vector3i grid_position, int lod_index, Array surface_arrays);
	void on_block_exit(Vector3i grid_position, int lod_index);

	//void on_area_edited(Rect3i box);

	int debug_get_block_count() const;

protected:
	void _notification(int p_what);

private:
	void remove_block(int block_index);
	void set_world(World *world);
	void clear_instances();
	void update_visibility();

	static void _bind_methods();

	struct Block {
		int layer_index;
		Vector3i grid_position;
		DirectMultiMeshInstance multimesh_instance;
	};

	struct Layer {
		int lod_index = 0;
		float density = 0.1f;
		float vertical_alignment = 1.f;
		float min_scale = 1.f;
		float max_scale = 1.f;
		float offset_along_normal = 0.f;
		bool random_vertical_flip = false;
		// float min_slope = 0.f;
		// float max_slope = 1.f;
		// float min_height = 0.f;
		// float max_height = 10000.f;

		// TODO lods?
		Ref<Mesh> mesh;

		// TODO Collision shapes

		HashMap<Vector3i, int, Vector3iHasher> blocks;
	};

	struct Lod {
		std::vector<int> layers;
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
