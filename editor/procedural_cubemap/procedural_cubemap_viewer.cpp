#include "procedural_cubemap_viewer.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/cubemap.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/sphere_mesh.h"
#include "../../util/godot/editor_scale.h"
#include "../blocky_library/model_viewer.h"

namespace zylann::voxel {

constexpr const char *g_cubemap_shader = R"(
shader_type spatial;
render_mode cull_disabled;

uniform samplerCube u_cubemap;

varying vec3 v_pos;

void vertex() {
	v_pos = VERTEX;
}

void fragment() {
	ALBEDO = texture(u_cubemap, v_pos).rgb;
}
)";

VoxelProceduralCubemapViewer::VoxelProceduralCubemapViewer() {
	const float editor_scale = EDSCALE;

	ZN_ModelViewer *viewer = memnew(ZN_ModelViewer);
	// viewer->set_h_size_flags(Container::SIZE_EXPAND_FILL);
	viewer->set_v_size_flags(Container::SIZE_EXPAND_FILL);
	viewer->set_custom_minimum_size(Vector2(100, 150 * editor_scale));
	viewer->set_camera_target_position(Vector3());
	viewer->set_zoom_max_level(8);
	viewer->set_zoom_range(0.6f, 1.25f);
	add_child(viewer);

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(g_cubemap_shader);

	Ref<ShaderMaterial> mesh_material;
	mesh_material.instantiate();
	mesh_material->set_shader(shader);
	_material = mesh_material;

	Ref<SphereMesh> mesh;
	mesh.instantiate();

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_material_override(mesh_material);
	viewer->get_viewer_root_node()->add_child(mesh_instance);
}

void VoxelProceduralCubemapViewer::set_cubemap(Ref<VoxelProceduralCubemap> cm) {
	if (_cubemap == cm) {
		return;
	}

	if (_cubemap.is_valid()) {
		_cubemap->disconnect(
				VoxelStringNames::get_singleton().updated,
				callable_mp(this, &VoxelProceduralCubemapViewer::on_cubemap_updated)
		);
	}

	_cubemap = cm;

	if (_cubemap.is_valid()) {
		_cubemap->connect(
				VoxelStringNames::get_singleton().updated,
				callable_mp(this, &VoxelProceduralCubemapViewer::on_cubemap_updated)
		);
	}

	on_cubemap_updated();
}

void VoxelProceduralCubemapViewer::on_cubemap_updated() {
	Ref<Cubemap> texture;
	if (_cubemap.is_valid()) {
		texture = _cubemap->create_texture();
	}
	_material->set_shader_parameter(VoxelStringNames::get_singleton().u_cubemap, texture);
}

void VoxelProceduralCubemapViewer::_bind_methods() {}

} // namespace zylann::voxel
