#include "voxel_blocky_model_viewer.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/classes/standard_material_3d.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/mouse_button.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"
#include "model_viewer.h"

namespace zylann::voxel {

namespace {

Ref<Mesh> make_axes_mesh() {
	PackedVector3Array vertices;
	vertices.resize(6);
	Span<Vector3> vertices_w(vertices.ptrw(), vertices.size());
	vertices_w[0] = Vector3(0, 0, 0);
	vertices_w[1] = Vector3(1, 0, 0);
	vertices_w[2] = Vector3(0, 0, 0);
	vertices_w[3] = Vector3(0, 1, 0);
	vertices_w[4] = Vector3(0, 0, 0);
	vertices_w[5] = Vector3(0, 0, 1);

	PackedColorArray colors;
	colors.resize(6);
	Span<Color> colors_w(colors.ptrw(), colors.size());
	const Color x_color(1, 0, 0);
	const Color y_color(0, 1, 0);
	const Color z_color(0, 0, 1);
	colors_w[0] = x_color;
	colors_w[1] = x_color;
	colors_w[2] = y_color;
	colors_w[3] = y_color;
	colors_w[4] = z_color;
	colors_w[5] = z_color;

	PackedInt32Array indices;
	indices.resize(6);
	Span<int> indices_w(indices.ptrw(), indices.size());
	for (int i = 0; i < indices.size(); ++i) {
		indices_w[i] = i;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

	return mesh;
}

// TODO Re-use this function in VoxelDebug?
Ref<Mesh> make_wireboxes_mesh(Span<const AABB> p_aabbs, Color p_color) {
	if (p_aabbs.size() == 0) {
		return Ref<Mesh>();
	}

	PackedVector3Array vertices;
	PackedColorArray colors;
	PackedInt32Array indices;

	const unsigned int indices_per_box = 12 * 2;
	const unsigned int vertex_count = p_aabbs.size() * 8;
	const unsigned int index_count = p_aabbs.size() * indices_per_box;

	vertices.resize(vertex_count);
	colors.resize(vertex_count);
	indices.resize(index_count);

	Span<Vector3> vertices_w(vertices.ptrw(), vertices.size());
	Span<Color> colors_w(colors.ptrw(), colors.size());
	Span<int> indices_w(indices.ptrw(), indices.size());

	unsigned int vi = 0;
	unsigned int ii = 0;

	//    7----6
	//   /|   /|
	//  4----5 |
	//  | 3--|-2
	//  |/   |/
	//  0----1

	for (const AABB aabb : p_aabbs) {
		vertices_w[vi + 0] = aabb.position;
		vertices_w[vi + 1] = aabb.position + Vector3(aabb.size.x, 0, 0);
		vertices_w[vi + 2] = aabb.position + Vector3(aabb.size.x, 0, aabb.size.z);
		vertices_w[vi + 3] = aabb.position + Vector3(0, 0, aabb.size.z);

		vertices_w[vi + 4] = aabb.position + Vector3(0, aabb.size.y, 0);
		vertices_w[vi + 5] = aabb.position + Vector3(aabb.size.x, aabb.size.y, 0);
		vertices_w[vi + 6] = aabb.position + Vector3(aabb.size.x, aabb.size.y, aabb.size.z);
		vertices_w[vi + 7] = aabb.position + Vector3(0, aabb.size.y, aabb.size.z);

		indices_w[ii + 0] = vi + 0;
		indices_w[ii + 1] = vi + 1;

		indices_w[ii + 2] = vi + 1;
		indices_w[ii + 3] = vi + 2;

		indices_w[ii + 4] = vi + 2;
		indices_w[ii + 5] = vi + 3;

		indices_w[ii + 6] = vi + 3;
		indices_w[ii + 7] = vi + 0;

		indices_w[ii + 8] = vi + 0;
		indices_w[ii + 9] = vi + 4;

		indices_w[ii + 10] = vi + 1;
		indices_w[ii + 11] = vi + 5;

		indices_w[ii + 12] = vi + 2;
		indices_w[ii + 13] = vi + 6;

		indices_w[ii + 14] = vi + 3;
		indices_w[ii + 15] = vi + 7;

		indices_w[ii + 16] = vi + 4;
		indices_w[ii + 17] = vi + 5;

		indices_w[ii + 18] = vi + 5;
		indices_w[ii + 19] = vi + 6;

		indices_w[ii + 20] = vi + 6;
		indices_w[ii + 21] = vi + 7;

		indices_w[ii + 22] = vi + 7;
		indices_w[ii + 23] = vi + 4;

		vi += 8;
		ii += indices_per_box;
	}

	for (Color &color : colors_w) {
		color = p_color;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

	return mesh;
}

} // namespace

VoxelBlockyModelViewer::VoxelBlockyModelViewer() {
	Ref<StandardMaterial3D> line_material;
	line_material.instantiate();
	line_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	line_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);

	const float editor_scale = EDSCALE;

	ZN_ModelViewer *viewer = memnew(ZN_ModelViewer);
	viewer->set_h_size_flags(Container::SIZE_EXPAND_FILL);
	viewer->set_v_size_flags(Container::SIZE_EXPAND_FILL);
	viewer->set_custom_minimum_size(Vector2(100, 150 * editor_scale));
	viewer->set_camera_distance(1.9f);
	add_child(viewer);

	Node *viewer_root = viewer->get_viewer_root_node();

	_mesh_instance = memnew(MeshInstance3D);
	viewer_root->add_child(_mesh_instance);

	_collision_boxes_mesh_instance = memnew(MeshInstance3D);
	_collision_boxes_mesh_instance->set_material_override(line_material);
	_mesh_instance->add_child(_collision_boxes_mesh_instance);

	MeshInstance3D *axes = memnew(MeshInstance3D);
	axes->set_mesh(make_axes_mesh());
	axes->set_position(Vector3(-0.002, -0.002, -0.002));
	axes->set_material_override(line_material);
	viewer_root->add_child(axes);

	VBoxContainer *side_toolbar = memnew(VBoxContainer);
	Button *rotate_x_button = memnew(Button);
	Button *rotate_y_button = memnew(Button);
	Button *rotate_z_button = memnew(Button);
	rotate_x_button->set_text(ZN_TTR("Rotate X"));
	rotate_y_button->set_text(ZN_TTR("Rotate Y"));
	rotate_z_button->set_text(ZN_TTR("Rotate Z"));
	rotate_x_button->set_tooltip_text(ZN_TTR("Rotate 90 degrees around X (clockwise)"));
	rotate_y_button->set_tooltip_text(ZN_TTR("Rotate 90 degrees around Y (clockwise)"));
	rotate_z_button->set_tooltip_text(ZN_TTR("Rotate 90 degrees around Z (clockwise)"));
	rotate_x_button->connect("pressed", callable_mp(this, &VoxelBlockyModelViewer::_on_rotate_x_button_pressed));
	rotate_y_button->connect("pressed", callable_mp(this, &VoxelBlockyModelViewer::_on_rotate_y_button_pressed));
	rotate_z_button->connect("pressed", callable_mp(this, &VoxelBlockyModelViewer::_on_rotate_z_button_pressed));
	side_toolbar->add_child(rotate_x_button);
	side_toolbar->add_child(rotate_y_button);
	side_toolbar->add_child(rotate_z_button);
	add_child(side_toolbar);

	set_process(true);
}

void VoxelBlockyModelViewer::set_model(Ref<VoxelBlockyModel> model) {
	if (_model.is_valid()) {
		_model->disconnect(VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelBlockyModelViewer::_on_model_changed));
	}

	_model = model;

	if (_model.is_valid()) {
		_model->connect(VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelBlockyModelViewer::_on_model_changed));
	}

	update_model();
}

void VoxelBlockyModelViewer::set_undo_redo(EditorUndoRedoManager *urm) {
	_undo_redo = urm;
}

void VoxelBlockyModelViewer::update_model() {
	ZN_ASSERT_RETURN(_model.is_valid());
	// Can be null
	Ref<Mesh> mesh = _model->get_preview_mesh();
	_mesh_instance->set_mesh(mesh);

	Ref<Mesh> collision_boxes_mesh = make_wireboxes_mesh(_model->get_collision_aabbs(), Color(0.3, 0.4, 1.0));
	_collision_boxes_mesh_instance->set_mesh(collision_boxes_mesh);
}

void VoxelBlockyModelViewer::rotate_model_90(Vector3i::Axis axis) {
	ZN_ASSERT_RETURN(_model.is_valid());
	ZN_ASSERT_RETURN(_undo_redo != nullptr);

	EditorUndoRedoManager &urm = *_undo_redo;

	const bool clockwise = true;

	Vector3 axis_vec;
	axis_vec[axis] = 1.0;

	urm.create_action(String("Rotate {0}").format(varray(_model->get_class())));
	urm.add_do_method(_model.ptr(), "rotate_90", axis, clockwise);
	urm.add_do_method(this, "add_rotation_anim", Basis().rotated(axis_vec, math::PI_32 / 2.0));
	urm.add_undo_method(_model.ptr(), "rotate_90", axis, !clockwise);
	urm.add_undo_method(this, "add_rotation_anim", Basis().rotated(axis_vec, -math::PI_32 / 2.0));
	urm.commit_action();
}

void VoxelBlockyModelViewer::add_rotation_anim(Basis basis) {
	_rotation_anim_basis = basis * _rotation_anim_basis;
}

#ifdef ZN_GODOT
void VoxelBlockyModelViewer::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		process(get_tree()->get_process_time());
	}
}
#endif

#ifdef ZN_GODOT_EXTENSION
void VoxelBlockyModelViewer::_process(double delta) {
	process(delta);
}
#endif

void VoxelBlockyModelViewer::process(float delta) {
	if (_rotation_anim_basis.is_equal_approx(Basis())) {
		return;
	}
	// Fake counter-rotation to show the feedback of rotating by 90 degrees, because rotating cubes without animation
	// isn't easy to distinguish
	_rotation_anim_basis = _rotation_anim_basis.slerp(Basis(), 0.25);
	if (_rotation_anim_basis.is_equal_approx(Basis())) {
		_mesh_instance->set_transform(Transform3D());
	} else {
		Transform3D trans;
		trans.origin -= Vector3(0.5, 0.5, 0.5);
		trans = Transform3D(_rotation_anim_basis, Vector3()) * trans;
		trans.origin += Vector3(0.5, 0.5, 0.5);
		_mesh_instance->set_transform(trans);
	}
}

void VoxelBlockyModelViewer::_on_model_changed() {
	update_model();
}

void VoxelBlockyModelViewer::_on_rotate_x_button_pressed() {
	rotate_model_90(Vector3i::AXIS_X);
}

void VoxelBlockyModelViewer::_on_rotate_y_button_pressed() {
	rotate_model_90(Vector3i::AXIS_Y);
}

void VoxelBlockyModelViewer::_on_rotate_z_button_pressed() {
	rotate_model_90(Vector3i::AXIS_Z);
}

void VoxelBlockyModelViewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_rotation_anim"), &VoxelBlockyModelViewer::add_rotation_anim);
}

} // namespace zylann::voxel
