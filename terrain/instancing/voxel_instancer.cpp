#include "voxel_instancer.h"
#include "../../edition/voxel_tool.h"
//#include "../../generators/graph/voxel_generator_graph.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../voxel_lod_terrain.h"

#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>
#include <scene/3d/spatial.h>
#include <scene/resources/primitive_meshes.h>

class VoxelInstancerRigidBody : public RigidBody {
	GDCLASS(VoxelInstancerRigidBody, RigidBody);

public:
	VoxelInstancerRigidBody() {
		set_mode(RigidBody::MODE_STATIC);
	}

	void set_block_index(int block_index) {
		_block_index = block_index;
	}

	void set_instance_index(int instance_index) {
		_instance_index = instance_index;
	}

	void attach(VoxelInstancer *parent) {
		_parent = parent;
	}

	void detach_and_destroy() {
		_parent = nullptr;
		queue_delete();
	}

	// Note, for this the body must switch to convex shapes
	// void detach_and_become_rigidbody() {
	// 	//...
	// }

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_UNPARENTED:
				// The user could queue_free() that node in game,
				// so we have to notify the instancer to remove the multimesh instance and pointer
				if (_parent != nullptr) {
					_parent->on_body_removed(_block_index, _instance_index);
					_parent = nullptr;
				}
				break;
		}
	}

private:
	VoxelInstancer *_parent = nullptr;
	int _block_index = -1;
	int _instance_index = -1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelInstancer::VoxelInstancer() {
	set_notify_transform(true);
	// TEST
	// Ref<CubeMesh> mesh1;
	// mesh1.instance();
	// mesh1->set_size(Vector3(0.5f, 0.5f, 0.5f));
	// int layer_index = add_layer(0);
	// set_layer_mesh(layer_index, mesh1);

	// Ref<CubeMesh> mesh2;
	// mesh2.instance();
	// mesh2->set_size(Vector3(1.0f, 2.0f, 1.0f));
	// layer_index = add_layer(2);
	// set_layer_mesh(layer_index, mesh2);

	// Ref<CubeMesh> mesh3;
	// mesh3.instance();
	// mesh3->set_size(Vector3(2.0f, 20.0f, 2.0f));
	// layer_index = add_layer(3);
	// set_layer_mesh(layer_index, mesh3);
}

VoxelInstancer::~VoxelInstancer() {
	// Destroy everything
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		memdelete(*it);
		// We don't destroy nodes, we assume they were detached already
	}
	for (auto it = _layers.begin(); it != _layers.end(); ++it) {
		Layer *layer = *it;
		if (layer != nullptr) {
			memdelete(layer);
		}
	}
}

void VoxelInstancer::clear_instances() {
	// Destroy blocks, keep configured layers
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		for (int i = 0; i < block->bodies.size(); ++i) {
			VoxelInstancerRigidBody *body = block->bodies[i];
			body->detach_and_destroy();
		}
		memdelete(block);
	}
	_blocks.clear();
	for (auto it = _layers.begin(); it != _layers.end(); ++it) {
		Layer *layer = *it;
		if (layer != nullptr) {
			layer->blocks.clear();
		}
	}
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		lod.modified_blocks.clear();
	}
}

void VoxelInstancer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD:
			set_world(*get_world());
			update_visibility();
			break;

		case NOTIFICATION_EXIT_WORLD:
			set_world(nullptr);
			break;

		case NOTIFICATION_PARENTED:
			_parent = Object::cast_to<VoxelLodTerrain>(get_parent());
			if (_parent != nullptr) {
				_parent->set_instancer(this);
			}
			break;

		case NOTIFICATION_UNPARENTED:
			clear_instances();
			if (_parent != nullptr) {
				_parent->set_instancer(nullptr);
			}
			_parent = nullptr;
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			VOXEL_PROFILE_SCOPE_NAMED("VoxelInstancer::NOTIFICATION_TRANSFORM_CHANGED");

			if (!is_inside_tree() || _parent == nullptr) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			const Transform parent_transform = get_global_transform();
			const int base_block_size_po2 = _parent->get_block_size_pow2();
			//print_line(String("IP: {0}").format(varray(parent_transform.origin)));

			for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
				Block *block = *it;
				const Layer *layer = _layers[block->layer_index];
				const int block_size_po2 = base_block_size_po2 + layer->lod_index;
				const Vector3 block_local_pos = (block->grid_position << block_size_po2).to_vec3();
				// The local block transform never has rotation or scale so we can take a shortcut
				const Transform block_transform(parent_transform.basis, parent_transform.xform(block_local_pos));
				block->multimesh_instance.set_transform(block_transform);
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			update_visibility();
			break;
	}
}

void VoxelInstancer::update_visibility() {
	if (!is_inside_tree()) {
		return;
	}
	const bool visible = is_visible_in_tree();
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		block->multimesh_instance.set_visible(visible);
	}
}

void VoxelInstancer::set_world(World *world) {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		block->multimesh_instance.set_world(world);
	}
}

void VoxelInstancer::set_up_mode(UpMode mode) {
	ERR_FAIL_COND(mode < 0 || mode >= UP_MODE_COUNT);
	_up_mode = mode;
}

VoxelInstancer::UpMode VoxelInstancer::get_up_mode() const {
	return _up_mode;
}

int VoxelInstancer::generate_persistent_id() const {
	int id = 0;
	for (size_t i = 0; i < _layers.size(); ++i) {
		const Layer *layer = _layers[i];
		if (layer != nullptr) {
			id = max(layer->persistent_id + 1, id);
			break;
		}
	}
	// Should not happen
	ERR_FAIL_COND_V(id > Layer::MAX_ID, -1);
	return id;
}

int VoxelInstancer::add_layer(int lod_index) {
	int layer_index = -1;
	for (size_t i = 0; i < _layers.size(); ++i) {
		if (_layers[i] == nullptr) {
			layer_index = i;
			break;
		}
	}
	if (layer_index == -1) {
		layer_index = _layers.size();
		_layers.push_back(nullptr);
	}

	Layer *layer = memnew(Layer);
	layer->lod_index = lod_index;

	if (Engine::get_singleton()->is_editor_hint()) {
		// Put a default generator
		layer->generator.instance();
	}

	_layers[layer_index] = layer;

	Lod &lod = _lods[lod_index];
	lod.layers.push_back(layer_index);

	return layer_index;
}

void VoxelInstancer::set_layer_generator(int layer_index, Ref<VoxelInstanceGenerator> generator) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->generator = generator;
}

void VoxelInstancer::set_layer_persistent(int layer_index, bool persistent) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	if (persistent) {
		layer->persistent = true;
		if (layer->persistent_id < 0 || find_layer_by_persistent_id(layer->persistent_id) != -1) {
			layer->persistent_id = generate_persistent_id();
		}
	} else {
		layer->persistent = false;
	}
}

void VoxelInstancer::set_layer_mesh(int layer_index, Ref<Mesh> mesh) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->mesh = mesh;
}

void VoxelInstancer::set_layer_collision_layer(int layer_index, int collision_layer) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->collision_layer = collision_layer;
}

void VoxelInstancer::set_layer_collision_mask(int layer_index, int collision_mask) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->collision_mask = collision_mask;
}

void VoxelInstancer::set_layer_material_override(int layer_index, Ref<Material> material) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->material_override = material;
}

void VoxelInstancer::set_layer_collision_shapes(int layer_index, Array shape_infos) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->collision_shapes.clear();

	for (int i = 0; i < shape_infos.size(); ++i) {
		Dictionary d = shape_infos[i];

		CollisionShapeInfo info;
		info.transform = d["transform"];
		info.shape = d["shape"];

		ERR_FAIL_COND(info.shape.is_null());

		layer->collision_shapes.push_back(info);
	}
}

// Configures a layer from an existing scene
// TODO Should expect a `Spatial`, but method bindings won't allow it
void VoxelInstancer::set_layer_from_template(int layer_index, Node *root) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	ERR_FAIL_COND(root == nullptr);

	layer->collision_shapes.clear();

	PhysicsBody *physics_body = Object::cast_to<PhysicsBody>(root);
	if (physics_body != nullptr) {
		layer->collision_layer = physics_body->get_collision_layer();
		layer->collision_mask = physics_body->get_collision_mask();
	}

	for (int i = 0; i < root->get_child_count(); ++i) {
		MeshInstance *mi = Object::cast_to<MeshInstance>(root->get_child(i));
		if (mi != nullptr) {
			layer->mesh = mi->get_mesh();
			layer->material_override = mi->get_material_override();
		}

		if (physics_body != nullptr) {
			CollisionShape *cs = Object::cast_to<CollisionShape>(physics_body->get_child(i));

			if (cs != nullptr) {
				CollisionShapeInfo info;
				info.shape = cs->get_shape();
				info.transform = cs->get_transform();

				layer->collision_shapes.push_back(info);
			}
		}
	}
}

void VoxelInstancer::remove_layer(int layer_index) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	Lod &lod = _lods[layer->lod_index];
	for (size_t i = 0; i < lod.layers.size(); ++i) {
		if (lod.layers[i] == layer_index) {
			lod.layers[i] = lod.layers.back();
			lod.layers.pop_back();
			break;
		}
	}

	Vector<int> to_remove;
	const Vector3i *block_pos_ptr = nullptr;
	while ((block_pos_ptr = layer->blocks.next(block_pos_ptr))) {
		int block_index = layer->blocks.get(*block_pos_ptr);
		to_remove.push_back(block_index);
	}

	for (int i = 0; i < to_remove.size(); ++i) {
		int block_index = to_remove[i];
		remove_block(block_index);
	}

	_layers[layer_index] = nullptr;
	memdelete(layer);
}

void VoxelInstancer::remove_block(int block_index) {
#ifdef DEBUG_ENABLED
	CRASH_COND(block_index < 0 || block_index >= _blocks.size());
#endif
	Block *moved_block = _blocks.back();
	Block *block = _blocks[block_index];
	{
		Layer *layer = _layers[block->layer_index];
		layer->blocks.erase(block->grid_position);
	}
	_blocks[block_index] = moved_block;
	_blocks.pop_back();

	for (int i = 0; i < block->bodies.size(); ++i) {
		VoxelInstancerRigidBody *body = block->bodies[i];
		body->detach_and_destroy();
	}

	memdelete(block);

	if (block != moved_block) {
#ifdef DEBUG_ENABLED
		CRASH_COND(moved_block->layer_index < 0 || moved_block->layer_index >= _layers.size());
#endif
		Layer *layer = _layers[moved_block->layer_index];
		int *iptr = layer->blocks.getptr(moved_block->grid_position);
		CRASH_COND(iptr == nullptr);
		*iptr = block_index;
	}
}

static inline Vector3 normalized(Vector3 pos, float &length) {
	length = pos.length();
	if (length == 0) {
		return Vector3();
	}
	pos.x /= length;
	pos.y /= length;
	pos.z /= length;
	return pos;
}

void VoxelInstancer::on_block_data_loaded(Vector3i grid_position, int lod_index,
		std::unique_ptr<VoxelInstanceBlockData> instances) {

	ERR_FAIL_COND(lod_index >= _lods.size());
	Lod &lod = _lods[lod_index];
	lod.loaded_instances_data.insert(std::make_pair(grid_position, std::move(instances)));
}

void VoxelInstancer::on_block_enter(Vector3i grid_position, int lod_index, Array surface_arrays) {
	if (lod_index >= _lods.size()) {
		return;
	}

	Lod &lod = _lods[lod_index];

	auto it = lod.loaded_instances_data.find(grid_position);
	const VoxelInstanceBlockData *instances_data = nullptr;

	if (it != lod.loaded_instances_data.end()) {
		instances_data = it->second.get();
		ERR_FAIL_COND(instances_data == nullptr);

		// We could do this:
		// lod.loaded_instances_data.erase(grid_position);
		// but we'd loose the information that this block was edited, so keeping it for now
	}

	create_blocks(instances_data, grid_position, lod_index, surface_arrays);
}

void VoxelInstancer::on_block_exit(Vector3i grid_position, int lod_index) {
	if (lod_index >= _lods.size()) {
		return;
	}

	VOXEL_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];

	// If we loaded data there but it was never used, we'll unload it either way
	lod.loaded_instances_data.erase(grid_position);

	if (lod.modified_blocks.has(grid_position)) {
		save_block(grid_position, lod_index);
		lod.modified_blocks.erase(grid_position);
	}

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_index = *it;

		Layer *layer = _layers[layer_index];
		CRASH_COND(layer == nullptr);

		int *block_index_ptr = layer->blocks.getptr(grid_position);
		if (block_index_ptr != nullptr) {
			remove_block(*block_index_ptr);
		}
	}
}

void VoxelInstancer::save_all_modified_blocks() {
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		const Vector3i *key = nullptr;
		while ((key = lod.modified_blocks.next(key))) {
			save_block(*key, lod_index);
		}
		lod.modified_blocks.clear();
	}
}

void VoxelInstancer::create_block_from_transforms(ArraySlice<const Transform> transforms,
		Vector3i grid_position, Layer *layer, unsigned int layer_index, World *world,
		const Transform &block_transform) {

	VOXEL_PROFILE_SCOPE();

	Ref<MultiMesh> multimesh;
	multimesh.instance();
	multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	multimesh->set_color_format(MultiMesh::COLOR_NONE);
	multimesh->set_custom_data_format(MultiMesh::CUSTOM_DATA_NONE);
	multimesh->set_mesh(layer->mesh);

	// Godot throws an error if we call `set_as_bulk_array` with an empty array
	if (transforms.size() > 0) {
		PoolRealArray bulk_array = DirectMultiMeshInstance::make_transform_3d_bulk_array(transforms);
		multimesh->set_instance_count(transforms.size());
		multimesh->set_as_bulk_array(bulk_array);
	}

	Block *block = memnew(Block);
	block->multimesh_instance.create();
	block->multimesh_instance.set_multimesh(multimesh);
	block->multimesh_instance.set_world(world);
	block->multimesh_instance.set_transform(block_transform);
	block->multimesh_instance.set_material_override(layer->material_override);
	block->layer_index = layer_index;
	block->grid_position = grid_position;
	const int block_index = _blocks.size();
	_blocks.push_back(block);

	if (layer->collision_shapes.size() > 0) {
		VOXEL_PROFILE_SCOPE();
		// Create bodies

		for (int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
			const Transform body_transform = block_transform * transforms[instance_index];

			VoxelInstancerRigidBody *body = memnew(VoxelInstancerRigidBody);
			body->attach(this);
			body->set_instance_index(instance_index);
			body->set_block_index(block_index);
			body->set_transform(body_transform);

			for (int i = 0; i < layer->collision_shapes.size(); ++i) {
				CollisionShapeInfo &shape_info = layer->collision_shapes.write[i];
				CollisionShape *cs = memnew(CollisionShape);
				cs->set_shape(shape_info.shape);
				cs->set_transform(shape_info.transform);
				body->add_child(cs);
			}

			add_child(body);
			block->bodies.push_back(body);
		}
	}

	layer->blocks.set(grid_position, block_index);
}

int VoxelInstancer::find_layer_by_persistent_id(int id) const {
	ERR_FAIL_COND_V(id < 0, -1);
	for (size_t i = 0; i < _layers.size(); ++i) {
		const Layer *layer = _layers[i];
		if (layer != nullptr && layer->persistent_id == id) {
			return i;
		}
	}
	return -1;
}

static bool contains_layer_id(const VoxelInstanceBlockData &instances_data, int id) {
	for (auto it = instances_data.layers.begin(); it != instances_data.layers.end(); ++it) {
		const VoxelInstanceBlockData::LayerData &layer = *it;
		if (layer.id == id) {
			return true;
		}
	}
	return false;
}

void VoxelInstancer::create_blocks(const VoxelInstanceBlockData *instances_data_ptr, Vector3i grid_position,
		int lod_index, Array surface_arrays) {

	VOXEL_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];
	const Transform parent_transform = get_global_transform();
	Ref<World> world_ref = get_world();
	ERR_FAIL_COND(world_ref.is_null());
	World *world = *world_ref;

	const int lod_block_size = _parent->get_block_size() << lod_index;
	const Transform block_local_transform = Transform(Basis(), (grid_position * lod_block_size).to_vec3());
	const Transform block_transform = parent_transform * block_local_transform;

	// Load from data if any
	if (instances_data_ptr != nullptr) {
		VOXEL_PROFILE_SCOPE();
		const VoxelInstanceBlockData &instances_data = *instances_data_ptr;

		for (auto layer_it = instances_data.layers.begin(); layer_it != instances_data.layers.end(); ++layer_it) {
			const VoxelInstanceBlockData::LayerData &layer_data = *layer_it;

			const int layer_index = find_layer_by_persistent_id(layer_data.id);
			if (layer_index == -1) {
				ERR_PRINT(String("Could not find associated layer ID {0} from loaded instances data")
								  .format(varray(layer_data.id)));
				continue;
			}

			Layer *layer = _layers[layer_index];
			CRASH_COND(layer == nullptr);

			if (!layer->persistent) {
				// That layer is no longer persistent so we'll have to ignore authored data...
				WARN_PRINT(String("Layer index={0} received loaded data but is no longer persistent. "
								  "Loaded data will be ignored.")
								   .format(varray(layer_index)));
				continue;
			}

			const int *block_index_ptr = layer->blocks.getptr(grid_position);
			if (block_index_ptr != nullptr) {
				// The block was already made?
				continue;
			}

			// TODO Don't create blocks if there are no transforms?

			static_assert(sizeof(VoxelInstanceBlockData::InstanceData) == sizeof(Transform),
					"Assuming instance data only contains a transform for now");
			ArraySlice<const Transform> transforms(
					reinterpret_cast<const Transform *>(layer_data.instances.data()), layer_data.instances.size());
			create_block_from_transforms(transforms, grid_position, layer, layer_index, world, block_transform);
		}
	}

	// Generate other blocks
	{
		VOXEL_PROFILE_SCOPE();

		if (surface_arrays.size() == 0) {
			return;
		}

		PoolVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];
		if (vertices.size() == 0) {
			return;
		}

		PoolVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];
		ERR_FAIL_COND(normals.size() == 0);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const int layer_index = *it;

			Layer *layer = _layers[layer_index];
			CRASH_COND(layer == nullptr);

			if (layer->generator.is_null()) {
				continue;
			}

			const int *block_index_ptr = layer->blocks.getptr(grid_position);
			if (block_index_ptr != nullptr) {
				// The block was already made?
				continue;
			}

			if (layer->persistent && instances_data_ptr != nullptr &&
					contains_layer_id(*instances_data_ptr, layer->persistent_id)) {
				// Don't generate, it received modified data
				continue;
			}

			_transform_cache.clear();

			layer->generator->generate_transforms(
					_transform_cache,
					grid_position,
					lod_index,
					layer_index,
					surface_arrays,
					block_local_transform,
					static_cast<VoxelInstanceGenerator::UpMode>(_up_mode));

			if (_transform_cache.size() == 0) {
				continue;
			}

			create_block_from_transforms(to_slice_const(_transform_cache),
					grid_position, layer, layer_index, world, block_transform);
		}
	}
}

static inline int get_visible_instance_count(Ref<MultiMesh> mm) {
	int visible_count = mm->get_visible_instance_count();
	if (visible_count == -1) {
		visible_count = mm->get_instance_count();
	}
	return visible_count;
}

void VoxelInstancer::save_block(Vector3i grid_pos, int lod_index) const {
	VOXEL_PROFILE_SCOPE();
	PRINT_VERBOSE(String("Requesting save of instance block {0} lod {1}")
						  .format(varray(grid_pos.to_vec3(), lod_index)));

	const Lod &lod = _lods[lod_index];

	std::unique_ptr<VoxelInstanceBlockData> data = std::make_unique<VoxelInstanceBlockData>();
	const int block_size = _parent->get_block_size() << lod_index;
	data->position_range = block_size;

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_index = *it;

		Layer *layer = _layers[layer_index];
		CRASH_COND(layer == nullptr);

		if (!layer->persistent) {
			continue;
		}
		ERR_FAIL_COND(layer->persistent_id < 0);

		int *block_index_ptr = layer->blocks.getptr(grid_pos);

		if (block_index_ptr != nullptr) {
			const int block_index = *block_index_ptr;

#ifdef DEBUG_ENABLED
			CRASH_COND(block_index < 0 || block_index >= _blocks.size());
#endif
			Block *block = _blocks[block_index];

			data->layers.push_back(VoxelInstanceBlockData::LayerData());
			VoxelInstanceBlockData::LayerData &layer_data = data->layers.back();

			Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
			CRASH_COND(multimesh.is_null());

			const int instance_count = get_visible_instance_count(multimesh);

			layer_data.id = layer->persistent_id;

			if (layer->generator.is_valid()) {
				layer_data.scale_min = layer->generator->get_min_scale();
				layer_data.scale_max = layer->generator->get_max_scale();
			} else {
				// TODO Calculate scale range automatically in the serializer
				layer_data.scale_min = 0.1f;
				layer_data.scale_max = 10.f;
			}

			layer_data.instances.resize(instance_count);

			// TODO Optimization: it would be nice to get the whole array at once
			for (int i = 0; i < instance_count; ++i) {
				VOXEL_PROFILE_SCOPE();
				layer_data.instances[i].transform = multimesh->get_instance_transform(i);
			}
		}
	}

	const int volume_id = _parent->get_volume_id();
	VoxelServer::get_singleton()->request_instance_block_save(volume_id, std::move(data), grid_pos, lod_index);
}

void VoxelInstancer::on_area_edited(Rect3i p_voxel_box) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int block_size = _parent->get_block_size();

	Ref<VoxelTool> voxel_tool = _parent->get_voxel_tool();
	voxel_tool->set_channel(VoxelBuffer::CHANNEL_SDF);

	const Transform parent_transform = get_global_transform();
	const int base_block_size_po2 = _parent->get_block_size_pow2();

	for (int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Rect3i blocks_box = p_voxel_box.downscaled(block_size << lod_index);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const Layer *layer = _layers[*it];
			const std::vector<Block *> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer->lod_index;

			blocks_box.for_each_cell([layer, &blocks, voxel_tool, p_voxel_box, parent_transform, block_size_po2, &lod](
											 Vector3i block_pos) {
				const int *iptr = layer->blocks.getptr(block_pos);
				if (iptr == nullptr) {
					// No instancing block here
					return;
				}

				Block *block = blocks[*iptr];
				Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();

				int initial_instance_count = get_visible_instance_count(multimesh);
				int instance_count = initial_instance_count;

				const Transform block_global_transform = Transform(parent_transform.basis,
						parent_transform.xform((block_pos << block_size_po2).to_vec3()));

				// Let's check all instances one by one
				// Note: the fact we have to query VisualServer in and out is pretty bad though.
				// - We probably have to sync with its thread in MT mode
				// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const Transform mm_transform = multimesh->get_instance_transform(instance_index);
					const Vector3i voxel_pos(mm_transform.origin + block_global_transform.origin);

					if (!p_voxel_box.contains(voxel_pos)) {
						continue;
					}

					// 1-voxel cheap check without interpolation
					const float sdf = voxel_tool->get_voxel_f(voxel_pos);
					if (sdf < -0.1f) {
						// Still enough ground
						continue;
					}

					// Remove the MultiMesh instance
					const int last_instance_index = --instance_count;
					const Transform last_trans = multimesh->get_instance_transform(last_instance_index);
					multimesh->set_instance_transform(instance_index, last_trans);

					// Remove the body if this block has some
					// TODO In the case of bodies, we could use an overlap check
					if (block->bodies.size() > 0) {
						VoxelInstancerRigidBody *rb = block->bodies[instance_index];
						// Detach so it won't try to update our instances, we already do it here
						rb->detach_and_destroy();

						VoxelInstancerRigidBody *moved_rb = block->bodies[last_instance_index];
						if (moved_rb != rb) {
							moved_rb->set_instance_index(instance_index);
							block->bodies.write[instance_index] = moved_rb;
						}
					}

					--instance_index;

					// DEBUG
					// Ref<CubeMesh> cm;
					// cm.instance();
					// cm->set_size(Vector3(0.5, 0.5, 0.5));
					// MeshInstance *mi = memnew(MeshInstance);
					// mi->set_mesh(cm);
					// mi->set_transform(get_global_transform() *
					// 				  (Transform(Basis(), (block_pos << layer->lod_index).to_vec3()) * t));
					// add_child(mi);
				}

				// All instances have to be frozen as edited.
				// Because even if none of them were removed or added, the ground on which they can spawn has changed,
				// and at the moment we don't want unexpected instances to generate when loading back this area.
				lod.modified_blocks.set(block_pos, true);

				if (instance_count < initial_instance_count) {
					// According to the docs, set_instance_count() resets the array so we only hide them instead
					multimesh->set_visible_instance_count(instance_count);

					if (block->bodies.size() > 0) {
						block->bodies.resize(instance_count);
					}

					// Array args;
					// args.push_back(instance_count);
					// args.push_back(initial_instance_count);
					// args.push_back(block_pos.to_vec3());
					// args.push_back(layer->lod_index);
					// args.push_back(multimesh->get_instance_count());
					// print_line(
					// 		String("Hiding instances from {0} to {1}. P: {2}, lod: {3}, total: {4}").format(args));
				}
			});
		}
	}
}

// This is called if a user destroys or unparents the body node while it's still attached to the ground
void VoxelInstancer::on_body_removed(int block_index, int instance_index) {
	ERR_FAIL_INDEX(block_index, _blocks.size());
	Block *block = _blocks[block_index];
	CRASH_COND(block == nullptr);
	ERR_FAIL_INDEX(instance_index, block->bodies.size());

	if (block->multimesh_instance.is_valid()) {
		// Remove the multimesh instance

		Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
		ERR_FAIL_COND(multimesh.is_null());

		int visible_count = get_visible_instance_count(multimesh);
		ERR_FAIL_COND(instance_index >= visible_count);

		--visible_count;
		const Transform last_trans = multimesh->get_instance_transform(visible_count);
		multimesh->set_instance_transform(instance_index, last_trans);
		multimesh->set_visible_instance_count(visible_count);
	}

	// Unregister the body
	int body_count = block->bodies.size();
	int last_instance_index = --body_count;
	VoxelInstancerRigidBody *moved_body = block->bodies[last_instance_index];
	if (instance_index != last_instance_index) {
		moved_body->set_instance_index(instance_index);
		block->bodies.write[instance_index] = moved_body;
	}
	block->bodies.resize(body_count);

	// Mark block as modified
	const Layer *layer = _layers[block->layer_index];
	CRASH_COND(layer == nullptr);
	Lod &lod = _lods[layer->lod_index];
	lod.modified_blocks.set(block->grid_position, true);
}

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
}

void VoxelInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_up_mode", "mode"), &VoxelInstancer::set_up_mode);
	ClassDB::bind_method(D_METHOD("get_up_mode"), &VoxelInstancer::get_up_mode);

	ClassDB::bind_method(D_METHOD("add_layer", "lod_index"), &VoxelInstancer::add_layer);

	ClassDB::bind_method(D_METHOD("set_layer_generator", "layer_index", "generator"),
			&VoxelInstancer::set_layer_generator);

	ClassDB::bind_method(D_METHOD("set_layer_persistent", "layer_index", "persistent"),
			&VoxelInstancer::set_layer_persistent);

	ClassDB::bind_method(D_METHOD("set_layer_mesh", "layer_index", "mesh"), &VoxelInstancer::set_layer_mesh);
	ClassDB::bind_method(D_METHOD("set_layer_mesh_material_override", "layer_index", "material"),
			&VoxelInstancer::set_layer_material_override);

	ClassDB::bind_method(D_METHOD("set_layer_collision_layer", "layer_index", "collision_layer"),
			&VoxelInstancer::set_layer_collision_layer);
	ClassDB::bind_method(D_METHOD("set_layer_collision_mask", "layer_index", "collision_mask"),
			&VoxelInstancer::set_layer_collision_mask);
	ClassDB::bind_method(D_METHOD("set_layer_collision_shapes", "layer_index", "shape_infos"),
			&VoxelInstancer::set_layer_collision_shapes);

	ClassDB::bind_method(D_METHOD("set_layer_from_template", "layer_index", "node"),
			&VoxelInstancer::set_layer_from_template);

	ClassDB::bind_method(D_METHOD("remove_layer", "layer_index"), &VoxelInstancer::remove_layer);

	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelInstancer::debug_get_block_count);

	BIND_CONSTANT(MAX_LOD);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);
}
