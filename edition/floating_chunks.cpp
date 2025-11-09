#include "floating_chunks.h"
#include "../constants/voxel_string_names.h"
#include "../storage/voxel_buffer.h"
#include "../util/godot/classes/array_mesh.h"
#include "../util/godot/classes/collision_shape_3d.h"
#include "../util/godot/classes/convex_polygon_shape_3d.h"
#include "../util/godot/classes/mesh_instance_3d.h"
#include "../util/godot/classes/rendering_server.h"
#include "../util/godot/classes/rigid_body_3d.h"
#include "../util/godot/classes/shader.h"
#include "../util/godot/classes/shader_material.h"
#include "../util/godot/classes/timer.h"
#include "../util/island_finder.h"
#include "../util/profiling.h"
#include "voxel_tool.h"

namespace zylann::voxel {

void box_propagate_ccl(Span<uint8_t> cells, const Vector3i size) {
	ZN_PROFILE_SCOPE();

	// Propagate non-zero cells towards zero cells in a 3x3x3 pattern.
	// Used on a grid produced by Connected-Component-Labelling.

	// Z
	{
		ZN_PROFILE_SCOPE_NAMED("Z");
		Vector3i pos;
		const int dz = size.x * size.y;
		unsigned int i = 0;
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = 0; pos.y < size.y; ++pos.y) {
				// Note, border cells are not handled. Not just because it's more work, but also because that could
				// make the label touch the edge, which is later interpreted as NOT being an island.
				pos.z = 2;
				i = Vector3iUtil::get_zxy_index(pos, size);
				for (; pos.z < size.z - 2; ++pos.z, i += dz) {
					const uint8_t c = cells[i];
					if (c != 0) {
						if (cells[i - dz] == 0) {
							cells[i - dz] = c;
						}
						if (cells[i + dz] == 0) {
							cells[i + dz] = c;
							// Skip next cell, otherwise it would cause endless propagation
							i += dz;
							++pos.z;
						}
					}
				}
			}
		}
	}

	// X
	{
		ZN_PROFILE_SCOPE_NAMED("X");
		Vector3i pos;
		const int dx = size.y;
		unsigned int i = 0;
		for (pos.z = 0; pos.z < size.z; ++pos.z) {
			for (pos.y = 0; pos.y < size.y; ++pos.y) {
				pos.x = 2;
				i = Vector3iUtil::get_zxy_index(pos, size);
				for (; pos.x < size.x - 2; ++pos.x, i += dx) {
					const uint8_t c = cells[i];
					if (c != 0) {
						if (cells[i - dx] == 0) {
							cells[i - dx] = c;
						}
						if (cells[i + dx] == 0) {
							cells[i + dx] = c;
							i += dx;
							++pos.x;
						}
					}
				}
			}
		}
	}

	// Y
	{
		ZN_PROFILE_SCOPE_NAMED("Y");
		Vector3i pos;
		const int dy = 1;
		unsigned int i = 0;
		for (pos.z = 0; pos.z < size.z; ++pos.z) {
			for (pos.x = 0; pos.x < size.x; ++pos.x) {
				pos.y = 2;
				i = Vector3iUtil::get_zxy_index(pos, size);
				for (; pos.y < size.y - 2; ++pos.y, i += dy) {
					const uint8_t c = cells[i];
					if (c != 0) {
						if (cells[i - dy] == 0) {
							cells[i - dy] = c;
						}
						if (cells[i + dy] == 0) {
							cells[i + dy] = c;
							i += dy;
							++pos.y;
						}
					}
				}
			}
		}
	}
}

// Turns floating chunks of voxels into rigidbodies:
// Detects separate groups of connected voxels within a box. Each group fully contained in the box is removed from
// the source volume, and turned into a rigidbody.
// This is one way of doing it, I don't know if it's the best way (there is rarely a best way)
// so there are probably other approaches that could be explored in the future, if they have better performance
Array separate_floating_chunks(
		VoxelTool &voxel_tool,
		Box3i world_box,
		Node *parent_node,
		Transform3D terrain_transform,
		Ref<VoxelMesher> mesher,
		Array materials
) {
	ZN_PROFILE_SCOPE();

	// Checks
	ERR_FAIL_COND_V(mesher.is_null(), Array());
	ERR_FAIL_COND_V(parent_node == nullptr, Array());

	// Copy source data

	// TODO Do not assume channel, at the moment it's hardcoded for smooth terrain
	static const int channels_mask = (1 << VoxelBuffer::CHANNEL_SDF);
	static const VoxelBuffer::ChannelId main_channel = VoxelBuffer::CHANNEL_SDF;

	VoxelBuffer source_copy_buffer(VoxelBuffer::ALLOCATOR_POOL);
	{
		ZN_PROFILE_SCOPE_NAMED("Copy");
		source_copy_buffer.create(world_box.size);
		voxel_tool.copy(world_box.position, source_copy_buffer, channels_mask, false);
	}

	// Label distinct voxel groups

	// TODO Candidate for temp allocator
	static thread_local StdVector<uint8_t> ccl_output;
	ccl_output.resize(Vector3iUtil::get_volume_u64(world_box.size));

	unsigned int label_count = 0;

	{
		// TODO Allow to run the algorithm at a different LOD, to trade precision for speed
		ZN_PROFILE_SCOPE_NAMED("CCL scan");
		IslandFinder island_finder;
		island_finder.scan_3d(
				Box3i(Vector3i(), world_box.size),
				[&source_copy_buffer](Vector3i pos) {
					// TODO Can be optimized further with direct access
					return source_copy_buffer.get_voxel_f(pos.x, pos.y, pos.z, main_channel) < 0.f;
				},
				to_span(ccl_output),
				&label_count
		);
	}

	struct Bounds {
		Vector3i min_pos;
		Vector3i max_pos; // inclusive
		bool valid = false;
	};

	if (main_channel == VoxelBuffer::CHANNEL_SDF) {
		// Propagate labels to improve SDF quality, otherwise gradients of separated chunks would cut off abruptly.
		// Limitation: if two islands are too close to each other, one will win over the other.
		// An alternative could be to do this on individual chunks?
		box_propagate_ccl(to_span(ccl_output), world_box.size);
	}

	// Compute bounds of each group

	StdVector<Bounds> bounds_per_label;
	{
		ZN_PROFILE_SCOPE_NAMED("Bounds calculation");

		// Adding 1 because label 0 is the index for "no label"
		bounds_per_label.resize(label_count + 1);

		unsigned int ccl_index = 0;
		for (int z = 0; z < world_box.size.z; ++z) {
			for (int x = 0; x < world_box.size.x; ++x) {
				for (int y = 0; y < world_box.size.y; ++y) {
					CRASH_COND(ccl_index >= ccl_output.size());
					const uint8_t label = ccl_output[ccl_index];
					++ccl_index;

					if (label == 0) {
						continue;
					}

					CRASH_COND(label >= bounds_per_label.size());
					Bounds &bounds = bounds_per_label[label];

					if (bounds.valid == false) {
						bounds.min_pos = Vector3i(x, y, z);
						bounds.max_pos = bounds.min_pos;
						bounds.valid = true;

					} else {
						if (x < bounds.min_pos.x) {
							bounds.min_pos.x = x;
						} else if (x > bounds.max_pos.x) {
							bounds.max_pos.x = x;
						}

						if (y < bounds.min_pos.y) {
							bounds.min_pos.y = y;
						} else if (y > bounds.max_pos.y) {
							bounds.max_pos.y = y;
						}

						if (z < bounds.min_pos.z) {
							bounds.min_pos.z = z;
						} else if (z > bounds.max_pos.z) {
							bounds.max_pos.z = z;
						}
					}
				}
			}
		}
	}

	// Eliminate groups that touch the box border,
	// because that means we can't tell if they are truly hanging in the air or attached to land further away

	const Vector3i lbmax = world_box.size - Vector3i(1, 1, 1);
	for (unsigned int label = 1; label < bounds_per_label.size(); ++label) {
		CRASH_COND(label >= bounds_per_label.size());
		Bounds &local_bounds = bounds_per_label[label];
		ERR_CONTINUE(!local_bounds.valid);

		if ( //
				local_bounds.min_pos.x == 0 //
				|| local_bounds.min_pos.y == 0 //
				|| local_bounds.min_pos.z == 0 //
				|| local_bounds.max_pos.x == lbmax.x //
				|| local_bounds.max_pos.y == lbmax.y //
				|| local_bounds.max_pos.z == lbmax.z) {
			//
			local_bounds.valid = false;
		}
	}

	// Create voxel buffer for each group

	struct InstanceInfo {
		VoxelBuffer voxels;
		Vector3i world_pos;
		unsigned int label;
	};
	StdVector<InstanceInfo> instances_info;

	const int min_padding = 2; // mesher->get_minimum_padding();
	const int max_padding = 2; // mesher->get_maximum_padding();

	{
		ZN_PROFILE_SCOPE_NAMED("Extraction");

		for (unsigned int label = 1; label < bounds_per_label.size(); ++label) {
			CRASH_COND(label >= bounds_per_label.size());
			const Bounds local_bounds = bounds_per_label[label];

			if (!local_bounds.valid) {
				continue;
			}

			const Vector3i world_pos = world_box.position + local_bounds.min_pos - Vector3iUtil::create(min_padding);
			const Vector3i size =
					local_bounds.max_pos - local_bounds.min_pos + Vector3iUtil::create(1 + max_padding + min_padding);

			instances_info.push_back(InstanceInfo{ VoxelBuffer(VoxelBuffer::ALLOCATOR_POOL), world_pos, label });

			VoxelBuffer &buffer = instances_info.back().voxels;
			buffer.create(size.x, size.y, size.z);

			// Read voxels from the source volume
			voxel_tool.copy(world_pos, buffer, channels_mask, false);

			// Cleanup padding borders
			const Box3i inner_box(
					Vector3iUtil::create(min_padding),
					buffer.get_size() - Vector3iUtil::create(min_padding + max_padding)
			);
			Box3i(Vector3i(), buffer.get_size()).difference(inner_box, [&buffer](Box3i box) {
				buffer.fill_area_f(constants::SDF_FAR_OUTSIDE, box.position, box.position + box.size, main_channel);
			});

			// Filter out voxels that don't belong to this label
			for (int z = local_bounds.min_pos.z; z <= local_bounds.max_pos.z; ++z) {
				for (int x = local_bounds.min_pos.x; x <= local_bounds.max_pos.x; ++x) {
					for (int y = local_bounds.min_pos.y; y <= local_bounds.max_pos.y; ++y) {
						const unsigned int ccl_index = Vector3iUtil::get_zxy_index(Vector3i(x, y, z), world_box.size);
						CRASH_COND(ccl_index >= ccl_output.size());
						const uint8_t label2 = ccl_output[ccl_index];

						if (label2 != 0 && label != label2) {
							buffer.set_voxel_f(
									constants::SDF_FAR_OUTSIDE,
									min_padding + x - local_bounds.min_pos.x,
									min_padding + y - local_bounds.min_pos.y,
									min_padding + z - local_bounds.min_pos.z,
									main_channel
							);
						}
					}
				}
			}
		}
	}

	// Erase voxels from source volume.
	// Must be done after we copied voxels from it.

	{
		ZN_PROFILE_SCOPE_NAMED("Erasing");

		voxel_tool.set_channel(main_channel);

		for (unsigned int instance_index = 0; instance_index < instances_info.size(); ++instance_index) {
			CRASH_COND(instance_index >= instances_info.size());
			const InstanceInfo &info = instances_info[instance_index];
			voxel_tool.sdf_stamp_erase(info.voxels, info.world_pos);
		}
	}

	// Find out which materials contain parameters that require instancing.
	//
	// Since 7dbc458bb4f3e0cc94e5070bd33bde41d214c98d it's no longer possible to quickly check if a
	// shader has a uniform by name using Shader's parameter cache. Now it seems the only way is to get the whole list
	// of parameters and find into it, which is slow, tedious to write and different between modules and GDExtension.

	uint32_t materials_to_instance_mask = 0;
	{
		StdVector<zylann::godot::ShaderParameterInfo> params;
		const String u_block_local_transform = VoxelStringNames::get_singleton().u_block_local_transform;

		ZN_ASSERT_RETURN_V_MSG(
				materials.size() < 32,
				Array(),
				"Too many materials. If you need more, make a request or change the code."
		);

		for (int material_index = 0; material_index < materials.size(); ++material_index) {
			Ref<ShaderMaterial> sm = materials[material_index];
			if (sm.is_null()) {
				continue;
			}

			Ref<Shader> shader = sm->get_shader();
			if (shader.is_null()) {
				continue;
			}

			params.clear();
			zylann::godot::get_shader_parameter_list(shader->get_rid(), params);

			for (const zylann::godot::ShaderParameterInfo &param_info : params) {
				if (param_info.name == u_block_local_transform) {
					materials_to_instance_mask |= (1 << material_index);
					break;
				}
			}
		}
	}

	// Create instances

	Array nodes;

	{
		ZN_PROFILE_SCOPE_NAMED("Remeshing and instancing");

		for (unsigned int instance_index = 0; instance_index < instances_info.size(); ++instance_index) {
			CRASH_COND(instance_index >= instances_info.size());
			const InstanceInfo &info = instances_info[instance_index];

			CRASH_COND(info.label >= bounds_per_label.size());
			const Bounds local_bounds = bounds_per_label[info.label];
			ERR_CONTINUE(!local_bounds.valid);

			// DEBUG
			// print_line(String("--- Instance {0}").format(varray(instance_index)));
			// for (int z = 0; z < info.voxels->get_size().z; ++z) {
			// 	for (int x = 0; x < info.voxels->get_size().x; ++x) {
			// 		String s;
			// 		for (int y = 0; y < info.voxels->get_size().y; ++y) {
			// 			float sdf = info.voxels->get_voxel_f(x, y, z, VoxelBuffer::CHANNEL_SDF);
			// 			if (sdf < -0.1f) {
			// 				s += "X ";
			// 			} else if (sdf < 0.f) {
			// 				s += "x ";
			// 			} else {
			// 				s += "- ";
			// 			}
			// 		}
			// 		print_line(s);
			// 	}
			// 	print_line("//");
			// }

			const Transform3D local_transform(
					Basis(),
					info.world_pos
							// Undo min padding
							+ Vector3i(1, 1, 1)
			);

			for (int i = 0; i < materials.size(); ++i) {
				if ((materials_to_instance_mask & (1 << i)) != 0) {
					Ref<ShaderMaterial> sm = materials[i];
					ZN_ASSERT_CONTINUE(sm.is_valid());
					sm = sm->duplicate(false);
					// That parameter should have a valid default value matching the local transform relative to the
					// volume, which is usually per-instance, but in Godot 3 we have no such feature, so we have to
					// duplicate.
					// TODO Try using per-instance parameters for scalar uniforms (Godot 4 doesn't support textures)
					sm->set_shader_parameter(
							VoxelStringNames::get_singleton().u_block_local_transform, local_transform
					);
					materials[i] = sm;
				}
			}

			// TODO If normalmapping is used here with the Transvoxel mesher, we need to either turn it off just for
			// this call, or to pass the right options
			Ref<ArrayMesh> mesh = mesher->build_mesh(info.voxels, materials, Dictionary());
			// The mesh is not supposed to be null,
			// because we build these buffers from connected groups that had negative SDF.
			ERR_CONTINUE(mesh.is_null());

			if (zylann::godot::is_mesh_empty(**mesh)) {
				continue;
			}

			// DEBUG
			// {
			// 	Ref<VoxelBlockSerializer> serializer;
			// 	serializer.instance();
			// 	Ref<StreamPeerBuffer> peer;
			// 	peer.instance();
			// 	serializer->serialize(peer, info.voxels, false);
			// 	String fpath = String("debug_data/split_dump_{0}.bin").format(varray(instance_index));
			// 	FileAccess *f = FileAccess::open(fpath, FileAccess::WRITE);
			// 	PoolByteArray bytes = peer->get_data_array();
			// 	PoolByteArray::Read bytes_read = bytes.read();
			// 	f->store_buffer(bytes_read.ptr(), bytes.size());
			// 	f->close();
			// 	memdelete(f);
			// }

			// TODO Option to make multiple convex shapes
			// TODO Use the fast way. This is slow because of the internal TriangleMesh thing and mesh data query.
			// TODO Don't create a body if the mesh has no triangles
			Ref<Shape3D> shape = mesh->create_convex_shape();
			ERR_CONTINUE(shape.is_null());
			CollisionShape3D *collision_shape = memnew(CollisionShape3D);
			collision_shape->set_shape(shape);
			// Center the shape somewhat, because Godot is confusing node origin with center of mass
			const Vector3i size =
					local_bounds.max_pos - local_bounds.min_pos + Vector3iUtil::create(1 + max_padding + min_padding);
			const Vector3 offset = -Vector3(size) * 0.5f;
			collision_shape->set_position(offset);

			RigidBody3D *rigid_body = memnew(RigidBody3D);
			rigid_body->set_transform(terrain_transform * local_transform.translated_local(-offset));
			rigid_body->add_child(collision_shape);
			rigid_body->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
			rigid_body->set_freeze_enabled(true);

			// Switch to rigid after a short time to workaround clipping with terrain,
			// because colliders are updated asynchronously
			Timer *timer = memnew(Timer);
			timer->set_wait_time(0.2);
			timer->set_one_shot(true);
			timer->connect("timeout", callable_mp(rigid_body, &RigidBody3D::set_freeze_enabled).bind(false));
			// Cannot use start() here because it requires to be inside the SceneTree,
			// and we don't know if it will be after we add to the parent.
			timer->set_autostart(true);
			rigid_body->add_child(timer);

			MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_mesh(mesh);
			mesh_instance->set_position(offset);
			rigid_body->add_child(mesh_instance);

			parent_node->add_child(rigid_body);

			nodes.append(rigid_body);
		}
	}

	return nodes;
}

} // namespace zylann::voxel
