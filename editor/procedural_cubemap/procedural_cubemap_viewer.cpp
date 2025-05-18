#include "procedural_cubemap_viewer.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/cubemap.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/resource_saver.h"
#include "../../util/godot/classes/sphere_mesh.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"
#include "../blocky_library/model_viewer.h"
#include "../instance_library/control_sizer.h"

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
	viewer->set_zoom_distance_range(0.6f, 1.25f);
	add_child(viewer);
	_viewer = viewer;

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

	ZN_ControlSizer *sizer = memnew(ZN_ControlSizer);
	sizer->set_target_control(viewer);
	add_child(sizer);

	Button *update_button = memnew(Button);
	update_button->set_text(ZN_TTR("Update"));
	update_button->connect("pressed", callable_mp(this, &VoxelProceduralCubemapViewer::on_update_button_pressed));
	add_child(update_button);

	Button *save_button = memnew(Button);
	save_button->set_text(ZN_TTR("Save to texture..."));
	save_button->connect("pressed", callable_mp(this, &VoxelProceduralCubemapViewer::on_save_button_pressed));
	add_child(save_button);

	_save_dialog = memnew(EditorFileDialog);
	_save_dialog->set_title("Save Cubemap Texture");
	_save_dialog->add_filter("*.res", ZN_TTR("Binary Resource"));
	_save_dialog->add_filter("*.tres", ZN_TTR("Text Resource (heavy)"));
	_save_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	_save_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	_save_dialog->connect(
			VoxelStringNames::get_singleton().file_selected,
			callable_mp(this, &VoxelProceduralCubemapViewer::on_save_dialog_file_selected)
	);
	add_child(_save_dialog);
}

void VoxelProceduralCubemapViewer::set_ctrl_to_zoom(const bool enable) {
	_viewer->set_ctrl_to_zoom(enable);
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

		if (_cubemap->is_dirty()) {
			_cubemap->update();
		}
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

void VoxelProceduralCubemapViewer::on_update_button_pressed() {
	ZN_ASSERT_RETURN(_cubemap.is_valid());
	_cubemap->update();
}

void VoxelProceduralCubemapViewer::on_save_button_pressed() {
	ZN_ASSERT_RETURN(_cubemap.is_valid());
	zylann::godot::popup_file_dialog(*_save_dialog);
}

void VoxelProceduralCubemapViewer::on_save_dialog_file_selected(const String &fpath) {
	ZN_ASSERT_RETURN(_cubemap.is_valid());
	Ref<Cubemap> texture = _cubemap->create_texture();
	ZN_ASSERT_RETURN(texture.is_valid());
	zylann::godot::save_resource(texture, fpath);
}

void VoxelProceduralCubemapViewer::_bind_methods() {}

} // namespace zylann::voxel
