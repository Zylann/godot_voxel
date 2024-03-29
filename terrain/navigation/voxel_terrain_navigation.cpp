#include "voxel_terrain_navigation.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/navigation_mesh_source_geometry_data_3d.h"
#include "../../util/godot/classes/navigation_region_3d.h"
#include "../../util/godot/classes/navigation_server_3d.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../fixed_lod/voxel_terrain.h"

#ifdef DEV_ENABLED
#include "../../util/string_funcs.h"
#endif

namespace zylann::voxel {

namespace {

#ifdef DEV_ENABLED

void dump_navmesh_source_data_to_mesh_resource(
		const PackedFloat32Array &vertices, const PackedInt32Array &indices, const char *fpath) {
	PackedVector3Array debug_vertices;
	for (int i = 0; i < vertices.size(); i += 3) {
		debug_vertices.push_back(Vector3(vertices[i], vertices[i + 1], vertices[i + 2]));
	}

	PackedInt32Array debug_indices = indices;
	for (int i0 = 1; i0 < debug_indices.size(); i0 += 3) {
		std::swap(debug_indices.write[i0], debug_indices.write[i0 + 1]);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = debug_vertices;
	arrays[Mesh::ARRAY_INDEX] = debug_indices;
	Ref<ArrayMesh> debug_mesh;
	debug_mesh.instantiate();
	debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	ResourceSaver::save(debug_mesh, fpath);
}

void check_navmesh_source_data(
		const PackedFloat32Array &vertices, const PackedInt32Array &indices, unsigned int vertex_count) {
	ZN_PROFILE_SCOPE();
	// Some sanity checks to make sure we don't send garbage to the baker

	for (int ii = 0; ii < indices.size(); ++ii) {
		const int32_t i = indices[ii];
		ZN_ASSERT(i >= 0 && i < vertices.size());
	}

	unsigned int used_vertices_count = 0;
	StdVector<bool> used_vertices;
	used_vertices.resize(vertices.size(), false);
	for (int ii = 0; ii < indices.size(); ++ii) {
		const int32_t i = indices[ii];
		if (!used_vertices[i]) {
			used_vertices[i] = true;
			++used_vertices_count;
		}
	}
	// Can't actually expect all vertices to be used, our implementation of Transvoxel has edge cases that can leave a
	// few vertices orphaned (nearly-degenerate triangles). So we'll check with reasonable margin instead
	ZN_ASSERT(used_vertices_count > (10 * vertex_count / 11));
}

size_t get_geometry_cache_estimated_size_in_bytes(const VoxelMeshMap<VoxelMeshBlockVT> &map) {
	size_t size = 0;
	map.for_each_block([&size](const VoxelMeshBlockVT &block) { //
		size += get_estimated_size_in_bytes(block.geometry_cache);
	});
	return size;
}

#endif

void copy_navigation_mesh_settings(NavigationMesh &dst, const NavigationMesh &src) {
	dst.set_agent_height(src.get_agent_height());
	dst.set_agent_max_climb(src.get_agent_max_climb());
	dst.set_agent_max_slope(src.get_agent_max_slope());
	// TODO `get_agent_radius` is not `const` due to an oversight in Godot
	dst.set_agent_radius(const_cast<NavigationMesh &>(src).get_agent_radius());

	// These are tricky because they must match the map... Instead of requiring the user to set them, we'll
	// automatically use the map's values instead.
	//
	// dst.set_cell_height(src.get_cell_height());
	// dst.set_cell_size(src.get_cell_size());

	dst.set_detail_sample_distance(src.get_detail_sample_distance());
	dst.set_detail_sample_max_error(src.get_detail_sample_max_error());
	dst.set_edge_max_error(src.get_edge_max_error());
	dst.set_edge_max_length(src.get_edge_max_length());
	dst.set_filter_baking_aabb(src.get_filter_baking_aabb());
	dst.set_filter_baking_aabb_offset(src.get_filter_baking_aabb_offset());
	dst.set_filter_ledge_spans(src.get_filter_ledge_spans());
	dst.set_collision_mask(src.get_collision_mask());
	dst.set_parsed_geometry_type(src.get_parsed_geometry_type());
	dst.set_source_geometry_mode(src.get_source_geometry_mode());
	dst.set_source_group_name(src.get_source_group_name());
	dst.set_region_merge_size(src.get_region_merge_size());
	dst.set_region_min_size(src.get_region_min_size());
	dst.set_sample_partition_type(src.get_sample_partition_type());
	dst.set_vertices_per_polygon(src.get_vertices_per_polygon());
}

void flip_triangle_indices_winding(Span<int32_t> indices) {
	ZN_PROFILE_SCOPE();
	for (unsigned int i0 = 1; i0 < indices.size(); i0 += 3) {
		std::swap(indices[i0], indices[i0 + 1]);
	}
}

Ref<NavigationMeshSourceGeometryData3D> gather_geometry(const VoxelTerrain &terrain) {
	ZN_PROFILE_SCOPE();

	struct VoxelMeshGeometry {
		PackedVector3Array vertices;
		PackedInt32Array indices;
		Vector3i block_position;
	};

	StdVector<VoxelMeshGeometry> geoms;

	{
		ZN_PROFILE_SCOPE_NAMED("Gather from terrain");

		const VoxelMeshMap<VoxelMeshBlockVT> &mesh_map = terrain.get_mesh_map();
		// TODO Could benefit from TempAllocator
		geoms.reserve(100);

		// Gather geometry from voxel terrain
		mesh_map.for_each_block([&geoms](const VoxelMeshBlockVT &block) {
			for (const VoxelMeshGeometryCache::Surface &surface : block.geometry_cache.surfaces) {
				geoms.push_back(VoxelMeshGeometry{ surface.vertices, surface.indices, block.position });
			}
		});

#ifdef DEBUG_ENABLED
		{
			ZN_PROFILE_SCOPE_NAMED("Size estimation");
			const size_t cache_size = get_geometry_cache_estimated_size_in_bytes(mesh_map);
			ZN_PRINT_VERBOSE(format("Mesh map geometry cache size: {}", cache_size));
		}
#endif
	}

	// TODO Also gather props
	// TODO Have the option to combine data with Godot's scene tree parser?
	// `NavigationServer::parse_source_geometry_data`

	Ref<NavigationMeshSourceGeometryData3D> navmesh_source_data;
	{
		ZN_PROFILE_SCOPE_NAMED("Processing");

		unsigned int vertex_count = 0;
		for (const VoxelMeshGeometry &geom : geoms) {
			vertex_count += geom.vertices.size();
		}

		unsigned int index_count = 0;
		for (const VoxelMeshGeometry &geom : geoms) {
			index_count += geom.indices.size();
		}

		PackedFloat32Array vertices;
		PackedInt32Array indices;

		// Resize in one go, it's faster than many push_backs
		vertices.resize(vertex_count * 3);
		indices.resize(index_count);

		Span<Vector3f> vertices_w = to_span(vertices).reinterpret_cast_to<Vector3f>();
		Span<int32_t> indices_w = to_span(indices);

		const int block_size = terrain.get_mesh_block_size();
		int32_t vertex_start = 0;

		// Combine meshes
		for (const VoxelMeshGeometry &geom : geoms) {
			Span<const Vector3> vertices_r = to_span(geom.vertices);
			Span<const int32_t> indices_r = to_span(geom.indices);

			// Chunk meshes are all aligned in a grid, so transforming vertices is simple
			const Vector3f origin = to_vec3f(geom.block_position * block_size);

			for (unsigned int vi = 0; vi < vertices_r.size(); ++vi) {
				const Vector3f v = to_vec3f(vertices_r[vi]) + origin;
				vertices_w[vi] = v;
			}

			for (unsigned int ii = 0; ii < indices_r.size(); ++ii) {
				indices_w[ii] = indices_r[ii] + vertex_start;
			}

			vertex_start += vertices_r.size();
			// Move spans forward by the amount of vertices and indices we just added
			vertices_w = vertices_w.sub(vertices_r.size());
			indices_w = indices_w.sub(indices_r.size());
		}

		// Reset span to cover all indices
		indices_w = to_span(indices);

		// For some reason winding is flipped, perhaps that's a requirement from Recast
		flip_triangle_indices_winding(indices_w);

#ifdef DEV_ENABLED
		if (geoms.size() > 0) {
			check_navmesh_source_data(vertices, indices, vertex_count);
		}
#endif

		navmesh_source_data.instantiate();
		// Directly set vertices, don't use any of the other methods, they do more work and aren't specialized to our
		// case
		navmesh_source_data->set_vertices(vertices);
		navmesh_source_data->set_indices(indices);
	}

	return navmesh_source_data;
}

} // namespace

VoxelTerrainNavigation::VoxelTerrainNavigation() {
	// TODO Eventually get rid of this node and use the server directly
	_navigation_region = memnew(NavigationRegion3D);
	add_child(_navigation_region);

	update_processing_state();
}

void VoxelTerrainNavigation::set_enabled(bool p_enabled) {
	_enabled = p_enabled;
	_navigation_region->set_enabled(p_enabled);
	update_processing_state();
}

void VoxelTerrainNavigation::set_run_in_editor(bool enable) {
	_run_in_editor = enable;
	update_processing_state();
}

bool VoxelTerrainNavigation::get_run_is_editor() const {
	return _run_in_editor;
}

void VoxelTerrainNavigation::update_processing_state() {
	bool enabled = _enabled;
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !_run_in_editor) {
		enabled = false;
	}
#endif
	set_process_internal(enabled);
}

bool VoxelTerrainNavigation::is_enabled() const {
	return _enabled;
}

void VoxelTerrainNavigation::set_template_navigation_mesh(Ref<NavigationMesh> navmesh) {
	_template_navmesh = navmesh;
#ifdef TOOLS_ENABLED
	update_configuration_warnings();
#endif
}

Ref<NavigationMesh> VoxelTerrainNavigation::get_template_navigation_mesh() const {
	return _template_navmesh;
}

void VoxelTerrainNavigation::debug_set_regions_visible_in_editor(bool enable) {
	if (_visible_regions_in_editor == enable) {
		return;
	}
	_visible_regions_in_editor = enable;
#ifdef TOOLS_ENABLED
	// Don't forget to turn this off before saving, or you'll end up saving runtime-generated nodes.
	if (is_inside_tree()) {
		if (_visible_regions_in_editor) {
			_navigation_region->set_owner(get_tree()->get_edited_scene_root());
		} else {
			_navigation_region->set_owner(nullptr);
		}

		// TODO The Godot Editor doesn't update the scene tree dock when the owner of a node changes.
		// One workaround is to switch to another scene tab and back.
		// Another workaround is to instantiate a node owned by the edited scene, and delete it immediately after,
		// which does update the scene tree, but DOES NOT update navmesh previews. And when I set the owner back to
		// null, navmesh previews don't go away until I tab in and out of the scene...
		//
		// Node *temp_node = memnew(Node);
		// add_child(temp_node);
		// temp_node->set_owner(get_tree()->get_edited_scene_root());
		// temp_node->queue_free();
	}
#endif
}

bool VoxelTerrainNavigation::debug_get_regions_visible_in_editor() const {
	return _visible_regions_in_editor;
}

void VoxelTerrainNavigation::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS:
			process(get_process_delta_time());
			break;
		default:
			break;
	}
}

void VoxelTerrainNavigation::process(float delta_time) {
	ZN_PROFILE_SCOPE();

	if (!_enabled) {
		return;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !_run_in_editor) {
		return;
	}
#endif

	if (_template_navmesh.is_null()) {
		return;
	}

	Node *parent_node = get_parent();
	ZN_ASSERT_RETURN(parent_node != nullptr);

	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(parent_node);
	if (terrain == nullptr) {
		return;
	}

	_time_before_update -= delta_time;
	if (_time_before_update <= 0.f) {
		if (!_baking) {
			_time_before_update = _update_period_seconds;

			const unsigned int mesh_updates_count = terrain->get_mesh_updates_count();
			if (mesh_updates_count != _prev_mesh_updates_count) {
				_prev_mesh_updates_count = mesh_updates_count;
				bake(*terrain);
			}
		}
	}
}

void VoxelTerrainNavigation::bake(const VoxelTerrain &terrain) {
	ZN_PROFILE_SCOPE();
	Ref<NavigationMeshSourceGeometryData3D> navmesh_source_data = gather_geometry(terrain);

	// dump_navmesh_source_data_to_mesh_resource(vertices, indices, "test_navmesh_source.tres");

	const RID map_rid = _navigation_region->get_navigation_map();

	NavigationServer3D &ns = *NavigationServer3D::get_singleton();
	const real_t map_cell_size = ns.map_get_cell_size(map_rid);
	const real_t map_cell_height = ns.map_get_cell_height(map_rid);

	Ref<NavigationMesh> navmesh;
	navmesh.instantiate();
	// These must match, so copy them directly...
	navmesh->set_cell_size(map_cell_size);
	navmesh->set_cell_height(map_cell_height);
	// Not using `duplicate` because it is much slower. We could also recycle navmeshes?
	copy_navigation_mesh_settings(**navmesh, **_template_navmesh);

	_baking = true;

	ns.bake_from_source_geometry_data_async(navmesh, navmesh_source_data,
			callable_mp_static(&VoxelTerrainNavigation::on_async_bake_complete_static)
					.bind(get_instance_id(), navmesh));
}

void VoxelTerrainNavigation::on_async_bake_complete_static(uint64_t p_object_id, Ref<NavigationMesh> navmesh) {
	const ObjectID object_id(p_object_id);
	Object *obj = ObjectDB::get_instance(object_id);
	if (obj == nullptr) {
		// Don't raise an error like Callable would. We'll drop the navmesh.
#ifdef DEBUG_ENABLED
		ZN_PRINT_VERBOSE(format("Navmesh async baking finished, but {} was destroyed in the meantime.",
				zylann::godot::get_class_name_str<VoxelTerrainNavigation>()));
#endif
		return;
	}
	VoxelTerrainNavigation *nav = Object::cast_to<VoxelTerrainNavigation>(obj);
	ZN_ASSERT_RETURN(nav != nullptr);
	nav->on_async_bake_complete(navmesh);
}

void VoxelTerrainNavigation::on_async_bake_complete(Ref<NavigationMesh> navmesh) {
	_baking = false;
	_navigation_region->set_navigation_mesh(navmesh);
}

#ifdef TOOLS_ENABLED

#if defined(ZN_GODOT)
PackedStringArray VoxelTerrainNavigation::get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#elif defined(ZN_GODOT_EXTENSION)
PackedStringArray VoxelTerrainNavigation::_get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#endif

void VoxelTerrainNavigation::get_configuration_warnings(PackedStringArray &warnings) const {
	if (_template_navmesh.is_null()) {
		warnings.append("No template is assigned, can't determine settings for baking.");
	}

	if (_template_navmesh.is_valid()) {
		// TODO These warnings can't be updated once the user fixes them.
		// NavigationMesh doesn't emit the `changed` signal when its properties are changed.
		const real_t default_cell_size = 0.25;
		const real_t default_cell_height = 0.25;
		if (!Math::is_equal_approx(_template_navmesh->get_cell_size(), default_cell_size)) {
			warnings.append(String(
					"{0} template cell_size has been modified. It won't be used: {1} automatically sets it to the "
					"map's cell size when baking.")
									.format(varray(NavigationMesh::get_class_static(),
											VoxelTerrainNavigation::get_class_static())));
		}
		if (!Math::is_equal_approx(_template_navmesh->get_cell_height(), default_cell_height)) {
			warnings.append(String(
					"{0} template cell_height has been modified. It won't be used: {1} automatically sets it to the "
					"map's cell height when baking.")
									.format(varray(NavigationMesh::get_class_static(),
											VoxelTerrainNavigation::get_class_static())));
		}
	}
}

#endif

void VoxelTerrainNavigation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &VoxelTerrainNavigation::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &VoxelTerrainNavigation::is_enabled);

	ClassDB::bind_method(D_METHOD("set_run_in_editor", "enabled"), &VoxelTerrainNavigation::set_run_in_editor);
	ClassDB::bind_method(D_METHOD("get_run_in_editor"), &VoxelTerrainNavigation::get_run_is_editor);

	ClassDB::bind_method(
			D_METHOD("set_template_navigation_mesh", "navmesh"), &VoxelTerrainNavigation::set_template_navigation_mesh);
	ClassDB::bind_method(
			D_METHOD("get_template_navigation_mesh"), &VoxelTerrainNavigation::get_template_navigation_mesh);

	ClassDB::bind_method(D_METHOD("debug_get_regions_visible_in_editor"),
			&VoxelTerrainNavigation::debug_get_regions_visible_in_editor);
	ClassDB::bind_method(D_METHOD("debug_set_regions_visible_in_editor", "enabled"),
			&VoxelTerrainNavigation::debug_set_regions_visible_in_editor);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_in_editor"), "set_run_in_editor", "get_run_in_editor");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "template_navigation_mesh", PROPERTY_HINT_RESOURCE_TYPE,
						 NavigationMesh::get_class_static(),
						 PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT),
			"set_template_navigation_mesh", "get_template_navigation_mesh");

	ADD_GROUP("Debug", "debug_");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_regions_visible_in_editor"), "debug_set_regions_visible_in_editor",
			"debug_get_regions_visible_in_editor");
}

} // namespace zylann::voxel
