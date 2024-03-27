#include "voxel_terrain_navigation.h"
#include "../../util/godot/classes/navigation_mesh_source_geometry_data_3d.h"
#include "../../util/godot/classes/navigation_region_3d.h"
#include "../../util/godot/classes/navigation_server_3d.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../fixed_lod/voxel_terrain.h"

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

Ref<NavigationMeshSourceGeometryData3D> gather_geometry(const VoxelTerrain &terrain) {
	ZN_PROFILE_SCOPE();

	struct VoxelMeshGeometry {
		PackedVector3Array vertices;
		PackedInt32Array indices;
		Vector3i block_position;
	};

	StdVector<VoxelMeshGeometry> geoms;

	const VoxelMeshMap<VoxelMeshBlockVT> &mesh_map = terrain.get_mesh_map();
	// TODO Could benefit from TempAllocator
	geoms.reserve(100);

	// Gather geometry from voxel terrain
	mesh_map.for_each_block([&geoms](const VoxelMeshBlockVT &block) {
		// TODO Don't query the render mesh, cache the info we need instead
		Ref<Mesh> mesh = block.get_mesh();
		if (mesh.is_null()) {
			return;
		}

		Array arrays = mesh->surface_get_arrays(0);
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];

		geoms.push_back(VoxelMeshGeometry{ vertices, indices, block.position });
	});

	// TODO Also gather props
	// TODO Have the option to combine data with Godot's scene tree parser
	// `NavigationServer::parse_source_geometry_data`

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

	// Flip winding
	for (unsigned int i0 = 1; i0 < indices_w.size(); i0 += 3) {
		std::swap(indices_w[i0], indices_w[i0 + 1]);
	}

#ifdef DEV_ENABLED
	check_navmesh_source_data(vertices, indices, vertex_count);
#endif

	Ref<NavigationMeshSourceGeometryData3D> navmesh_source_data;
	navmesh_source_data.instantiate();
	navmesh_source_data->set_vertices(vertices);
	navmesh_source_data->set_indices(indices);

	return navmesh_source_data;
}

} // namespace

VoxelTerrainNavigation::VoxelTerrainNavigation() {
	// TODO Eventually get rid of this node and use the server directly
	_navigation_region = memnew(NavigationRegion3D);
	add_child(_navigation_region);

	set_process_internal(_enabled);
}

void VoxelTerrainNavigation::set_enabled(bool enabled) {
	_enabled = enabled;
	_navigation_region->set_enabled(enabled);
	set_process_internal(_enabled);
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

	print_line(String("Navmesh cell_size: {0}").format(varray(navmesh->get_cell_size())));
	print_line(String("Navmesh cell_height: {0}").format(varray(navmesh->get_cell_height())));

	print_line(String("Map cell_size: {0}").format(varray(map_cell_size)));
	print_line(String("Map cell_height: {0}").format(varray(map_cell_height)));

	{
		ZN_PROFILE_SCOPE_NAMED("bake_from_source_geometry_data");
		// TODO Bake asynchronously
		_baking = true;
		ns.bake_from_source_geometry_data(navmesh, navmesh_source_data);
		_baking = false;
	}

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
}

#endif

void VoxelTerrainNavigation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &VoxelTerrainNavigation::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &VoxelTerrainNavigation::is_enabled);

	ClassDB::bind_method(
			D_METHOD("set_template_navigation_mesh", "navmesh"), &VoxelTerrainNavigation::set_template_navigation_mesh);
	ClassDB::bind_method(
			D_METHOD("get_template_navigation_mesh"), &VoxelTerrainNavigation::get_template_navigation_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "template_navigation_mesh", PROPERTY_HINT_RESOURCE_TYPE,
						 NavigationMesh::get_class_static(),
						 PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT),
			"set_template_navigation_mesh", "get_template_navigation_mesh");
}

} // namespace zylann::voxel
