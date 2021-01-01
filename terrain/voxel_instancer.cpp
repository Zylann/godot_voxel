#include "voxel_instancer.h"
#include "../edition/voxel_tool.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../util/profiling.h"
#include "voxel_lod_terrain.h"

#include <scene/3d/spatial.h>
#include <scene/resources/primitive_meshes.h>

VoxelInstancer::VoxelInstancer() {
	set_notify_transform(true);
	// TEST
	// Ref<CubeMesh> mesh1;
	// mesh1.instance();
	// mesh1->set_size(Vector3(0.5f, 0.5f, 0.5f));
	// int layer_id = add_layer(0);
	// set_layer_mesh(layer_id, mesh1);

	// Ref<CubeMesh> mesh2;
	// mesh2.instance();
	// mesh2->set_size(Vector3(1.0f, 2.0f, 1.0f));
	// layer_id = add_layer(2);
	// set_layer_mesh(layer_id, mesh2);

	// Ref<CubeMesh> mesh3;
	// mesh3.instance();
	// mesh3->set_size(Vector3(2.0f, 20.0f, 2.0f));
	// layer_id = add_layer(3);
	// set_layer_mesh(layer_id, mesh3);
}

VoxelInstancer::~VoxelInstancer() {
	// Destroy everything
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		memdelete(*it);
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
		memdelete(*it);
	}
	_blocks.clear();
	for (auto it = _layers.begin(); it != _layers.end(); ++it) {
		Layer *layer = *it;
		if (layer != nullptr) {
			layer->blocks.clear();
		}
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

int VoxelInstancer::add_layer(int lod_index) {
	int layer_id = -1;
	for (size_t i = 0; i < _layers.size(); ++i) {
		if (_layers[i] == nullptr) {
			layer_id = i;
			break;
		}
	}
	if (layer_id == -1) {
		layer_id = _layers.size();
		_layers.push_back(nullptr);
	}

	Layer *layer = memnew(Layer);
	layer->lod_index = lod_index;
	_layers[layer_id] = layer;

	Lod &lod = _lods[lod_index];
	lod.layers.push_back(layer_id);

	return layer_id;
}

void VoxelInstancer::set_layer_mesh(int layer_index, Ref<Mesh> mesh) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->mesh = mesh;
}

void VoxelInstancer::set_layer_random_vertical_flip(int layer_index, bool flip_enabled) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->random_vertical_flip = flip_enabled;
}

void VoxelInstancer::set_layer_density(int layer_index, float density) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->density = max(density, 0.f);
}

void VoxelInstancer::set_layer_min_scale(int layer_index, float min_scale) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->min_scale = max(min_scale, 0.01f);
}

void VoxelInstancer::set_layer_max_scale(int layer_index, float max_scale) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->max_scale = max(max_scale, 0.01f);
}

void VoxelInstancer::set_layer_vertical_alignment(int layer_index, float amount) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->vertical_alignment = clamp(amount, 0.f, 1.f);
}

void VoxelInstancer::set_layer_offset_along_normal(int layer_index, float offset) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->offset_along_normal = offset;
}

void VoxelInstancer::set_layer_min_slope_degrees(int layer_index, float degrees) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->max_surface_normal_y = min(1.f, Math::cos(Math::deg2rad(clamp(degrees, -180.f, 180.f))));
}

void VoxelInstancer::set_layer_max_slope_degrees(int layer_index, float degrees) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->min_surface_normal_y = max(-1.f, Math::cos(Math::deg2rad(clamp(degrees, -180.f, 180.f))));
}

void VoxelInstancer::set_layer_min_height(int layer_index, float h) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->min_height = h;
}

void VoxelInstancer::set_layer_max_height(int layer_index, float h) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->max_height = h;
}

void VoxelInstancer::set_layer_material_override(int layer_index, Ref<Material> material) {
	ERR_FAIL_INDEX(layer_index, _layers.size());
	Layer *layer = _layers[layer_index];
	ERR_FAIL_COND(layer == nullptr);

	layer->material_override = material;
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

void VoxelInstancer::on_block_enter(Vector3i grid_position, int lod_index, Array surface_arrays) {
	if (lod_index >= _lods.size()) {
		return;
	}

	if (surface_arrays.size() == 0) {
		return;
	}

	PoolVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];

	if (vertices.size() == 0) {
		return;
	}

	VOXEL_PROFILE_SCOPE();

	PoolVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];

	//Ref<VoxelGeneratorGraph> graph_generator = _parent->get_stream();

	Lod &lod = _lods[lod_index];
	const Transform parent_transform = get_global_transform();
	Ref<World> world_ref = get_world();
	ERR_FAIL_COND(world_ref.is_null());
	World *world = *world_ref;

	const int base_block_size = _parent->get_block_size();
	const int lod_block_size = base_block_size << lod_index;
	const Transform block_local_transform = Transform(Basis(), (grid_position * lod_block_size).to_vec3());
	const Transform block_transform = parent_transform * block_local_transform;

	const uint32_t block_pos_hash = Vector3iHasher::hash(grid_position);

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_index = *it;

		Layer *layer = _layers[layer_index];
		CRASH_COND(layer == nullptr);

		const int *block_index_ptr = layer->blocks.getptr(grid_position);

		if (block_index_ptr != nullptr) {
			// The block was already made?
			continue;
		}

		const uint32_t density_u32 = 0xffffffff * layer->density;
		const float vertical_alignment = layer->vertical_alignment;
		const float scale_min = layer->min_scale;
		const float scale_range = layer->max_scale - layer->min_scale;
		const bool random_vertical_flip = layer->random_vertical_flip;
		const float offset_along_normal = layer->offset_along_normal;
		const float normal_min_y = layer->min_surface_normal_y;
		const float normal_max_y = layer->max_surface_normal_y;
		const bool slope_filter = normal_min_y != -1.f || normal_max_y != 1.f;
		const bool height_filter = layer->min_height != std::numeric_limits<float>::min() ||
								   layer->max_height != std::numeric_limits<float>::max();
		const float min_height = layer->min_height;
		const float max_height = layer->max_height;

		Vector3 global_up(0.f, 1.f, 0.f);

		// Using different number generators so changing parameters affecting one doesn't affect the other
		const uint64_t seed = block_pos_hash + layer_index;
		RandomPCG pcg0;
		pcg0.seed(seed);
		RandomPCG pcg1;
		pcg1.seed(seed + 1);

		_transform_cache.clear();
		{
			VOXEL_PROFILE_SCOPE();

			PoolVector3Array::Read vr = vertices.read();
			PoolVector3Array::Read nr = normals.read();

			// TODO This part might be moved to the meshing thread if it turns out to be too heavy

			for (size_t i = 0; i < vertices.size(); ++i) {
				// TODO We could actually generate indexes and pick those, rather than iterating them all and rejecting
				if (pcg0.rand() >= density_u32) {
					continue;
				}

				Transform t;
				t.origin = vr[i];

				// TODO Check if that position has been edited somehow, so we can decide to not spawn there
				// Or remesh from generator and compare sdf but that's expensive

				Vector3 axis_y;

				// Warning: sometimes mesh normals are not perfectly normalized.
				// The cause is for meshing speed on CPU. It's normalized on GPU anyways.
				Vector3 surface_normal = nr[i];
				bool surface_normal_is_normalized = false;
				bool sphere_up_is_computed = false;
				bool sphere_distance_is_computed = false;
				float sphere_distance;

				if (vertical_alignment == 0.f) {
					surface_normal.normalize();
					surface_normal_is_normalized = true;
					axis_y = surface_normal;

				} else {
					if (_up_mode == UP_MODE_SPHERE) {
						global_up = normalized(block_local_transform.origin + t.origin, sphere_distance);
						sphere_up_is_computed = true;
						sphere_distance_is_computed = true;
					}

					if (vertical_alignment < 1.f) {
						axis_y = surface_normal.linear_interpolate(global_up, vertical_alignment).normalized();

					} else {
						axis_y = global_up;
					}
				}

				if (slope_filter) {
					if (!surface_normal_is_normalized) {
						surface_normal.normalize();
					}

					float ny = surface_normal.y;
					if (_up_mode == UP_MODE_SPHERE) {
						if (!sphere_up_is_computed) {
							global_up = normalized(block_local_transform.origin + t.origin, sphere_distance);
							sphere_up_is_computed = true;
							sphere_distance_is_computed = true;
						}
						ny = surface_normal.dot(global_up);
					}

					if (ny < normal_min_y || ny > normal_max_y) {
						// Discard
						continue;
					}
				}

				if (height_filter) {
					float y = t.origin.y;
					if (_up_mode == UP_MODE_SPHERE) {
						if (!sphere_distance_is_computed) {
							sphere_distance = (block_local_transform.origin + t.origin).length();
							sphere_distance_is_computed = true;
						}
						y = sphere_distance;
					}

					if (y < min_height || y > max_height) {
						continue;
					}
				}

				t.origin += offset_along_normal * axis_y;

				// Allows to use two faces of a single rock to create variety in the same layer
				if (random_vertical_flip && (pcg1.rand() & 1) == 1) {
					axis_y = -axis_y;
					// TODO Should have to flip another axis as well?
				}

				// Pick a random rotation from the floor's normal.
				// TODO A pool of precomputed random directions would do the job too
				const Vector3 dir = Vector3(pcg1.randf() - 0.5f, pcg1.randf() - 0.5f, pcg1.randf() - 0.5f);
				const Vector3 axis_x = axis_y.cross(dir).normalized();
				const Vector3 axis_z = axis_x.cross(axis_y);

				t.basis = Basis(
						Vector3(axis_x.x, axis_y.x, axis_z.x),
						Vector3(axis_x.y, axis_y.y, axis_z.y),
						Vector3(axis_x.z, axis_y.z, axis_z.z));

				if (scale_range > 0.f) {
					const float scale = scale_min + scale_range * pcg1.randf();
					t.basis.scale(Vector3(scale, scale, scale));

				} else if (scale_min != 1.f) {
					t.basis.scale(Vector3(scale_min, scale_min, scale_min));
				}

				_transform_cache.push_back(t);
			}
		}

		if (_transform_cache.size() == 0) {
			continue;
		}

		// TODO Investigate if this helps (won't help with authored terrain)
		// if (graph_generator.is_valid()) {
		// 	for (size_t i = 0; i < _transform_cache.size(); ++i) {
		// 		Transform &t = _transform_cache[i];
		// 		const Vector3 up = t.get_basis().get_axis(Vector3::AXIS_Y);
		// 		t.origin = graph_generator->approximate_surface(t.origin, up * 0.5f);
		// 	}
		// }

		Ref<MultiMesh> multimesh;
		multimesh.instance();
		multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
		multimesh->set_color_format(MultiMesh::COLOR_NONE);
		multimesh->set_custom_data_format(MultiMesh::CUSTOM_DATA_NONE);
		multimesh->set_mesh(layer->mesh);

		PoolRealArray bulk_array;
		bulk_array.resize(_transform_cache.size() * 12);
		CRASH_COND(_transform_cache.size() * sizeof(Transform) / sizeof(float) != bulk_array.size());
		{
			VOXEL_PROFILE_SCOPE();
			PoolRealArray::Write w = bulk_array.write();

			//memcpy(w.ptr(), _transform_cache.data(), bulk_array.size() * sizeof(float));
			// Nope, you can't memcpy that, nonono. They say it's for performance.

			for (size_t i = 0; i < _transform_cache.size(); ++i) {
				float *ptr = w.ptr() + 12 * i;
				const Transform &t = _transform_cache[i];

				ptr[0] = t.basis.elements[0].x;
				ptr[1] = t.basis.elements[0].y;
				ptr[2] = t.basis.elements[0].z;
				ptr[3] = t.origin.x;

				ptr[4] = t.basis.elements[1].x;
				ptr[5] = t.basis.elements[1].y;
				ptr[6] = t.basis.elements[1].z;
				ptr[7] = t.origin.y;

				ptr[8] = t.basis.elements[2].x;
				ptr[9] = t.basis.elements[2].y;
				ptr[10] = t.basis.elements[2].z;
				ptr[11] = t.origin.z;
			}
		}

		multimesh->set_instance_count(_transform_cache.size());
		multimesh->set_as_bulk_array(bulk_array);

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

		layer->blocks.set(grid_position, block_index);
	}
}

void VoxelInstancer::on_block_exit(Vector3i grid_position, int lod_index) {
	if (lod_index >= _lods.size()) {
		return;
	}

	const Lod &lod = _lods[lod_index];

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

void VoxelInstancer::on_area_edited(Rect3i p_box) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int block_size = _parent->get_block_size();

	Ref<VoxelTool> voxel_tool = _parent->get_voxel_tool();
	voxel_tool->set_channel(VoxelBuffer::CHANNEL_SDF);

	const Transform parent_transform = get_global_transform();
	const int base_block_size_po2 = _parent->get_block_size_pow2();

	for (int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		const Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Rect3i blocks_box = p_box.downscaled(block_size << lod_index);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const Layer *layer = _layers[*it];
			const std::vector<Block *> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer->lod_index;

			blocks_box.for_each_cell([layer, &blocks, voxel_tool, p_box, parent_transform, block_size_po2](
											 Vector3i block_pos) {
				const int *iptr = layer->blocks.getptr(block_pos);

				if (iptr != nullptr) {
					Block *block = blocks[*iptr];
					Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();

					int initial_instance_count = multimesh->get_visible_instance_count();
					if (initial_instance_count == -1) {
						initial_instance_count = multimesh->get_instance_count();
					}

					int instance_count = initial_instance_count;

					const Transform block_global_transform = Transform(parent_transform.basis,
							parent_transform.xform((block_pos << block_size_po2).to_vec3()));

					// Let's check all instances one by one
					// Note: the fact we have to query VisualServer in and out is pretty bad though.
					// - We probably have to sync with its thread in MT mode
					// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
					for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
						const Transform mm_transform = multimesh->get_instance_transform(instance_index);
						const Vector3i voxel_pos = Vector3i(mm_transform.origin + block_global_transform.origin);

						if (p_box.contains(voxel_pos)) {
							// 1-voxel cheap check without interpolation
							const float sdf = voxel_tool->get_voxel_f(voxel_pos);

							if (sdf >= -0.1f) {
								// Remove the instance
								--instance_count;
								const Transform last_trans = multimesh->get_instance_transform(instance_count);
								multimesh->set_instance_transform(instance_index, last_trans);
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
						}
					}

					if (instance_count < initial_instance_count) {
						// According to the docs, set_instance_count() resets the array so we only hide them instead
						multimesh->set_visible_instance_count(instance_count);

						// Array args;
						// args.push_back(instance_count);
						// args.push_back(initial_instance_count);
						// args.push_back(block_pos.to_vec3());
						// args.push_back(layer->lod_index);
						// args.push_back(multimesh->get_instance_count());
						// print_line(
						// 		String("Hiding instances from {0} to {1}. P: {2}, lod: {3}, total: {4}").format(args));
					}
				}
			});
		}
	}
}

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
}

void VoxelInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_up_mode", "mode"), &VoxelInstancer::set_up_mode);
	ClassDB::bind_method(D_METHOD("get_up_mode"), &VoxelInstancer::get_up_mode);

	ClassDB::bind_method(D_METHOD("add_layer", "lod_index"), &VoxelInstancer::add_layer);
	ClassDB::bind_method(D_METHOD("set_layer_mesh", "layer_index", "mesh"), &VoxelInstancer::set_layer_mesh);
	ClassDB::bind_method(D_METHOD("set_layer_mesh_material_override", "layer_index", "material"),
			&VoxelInstancer::set_layer_material_override);
	ClassDB::bind_method(D_METHOD("set_layer_density", "layer_index", "density"), &VoxelInstancer::set_layer_density);
	ClassDB::bind_method(D_METHOD("set_layer_min_scale", "layer_index", "min_scale"),
			&VoxelInstancer::set_layer_min_scale);
	ClassDB::bind_method(D_METHOD("set_layer_max_scale", "layer_index", "max_scale"),
			&VoxelInstancer::set_layer_max_scale);
	ClassDB::bind_method(D_METHOD("set_layer_vertical_alignment", "layer_index", "amount"),
			&VoxelInstancer::set_layer_vertical_alignment);
	ClassDB::bind_method(D_METHOD("set_layer_random_vertical_flip", "layer_index", "enabled"),
			&VoxelInstancer::set_layer_random_vertical_flip);
	ClassDB::bind_method(D_METHOD("set_layer_offset_along_normal", "layer_index", "offset"),
			&VoxelInstancer::set_layer_offset_along_normal);
	ClassDB::bind_method(D_METHOD("set_layer_min_slope_degrees", "layer_index", "degrees"),
			&VoxelInstancer::set_layer_min_slope_degrees);
	ClassDB::bind_method(D_METHOD("set_layer_max_slope_degrees", "layer_index", "degrees"),
			&VoxelInstancer::set_layer_max_slope_degrees);
	ClassDB::bind_method(D_METHOD("set_layer_min_height", "layer_index", "height"),
			&VoxelInstancer::set_layer_min_height);
	ClassDB::bind_method(D_METHOD("set_layer_max_height", "layer_index", "height"),
			&VoxelInstancer::set_layer_max_height);
	ClassDB::bind_method(D_METHOD("remove_layer", "layer_index"), &VoxelInstancer::remove_layer);

	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelInstancer::debug_get_block_count);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);
}
