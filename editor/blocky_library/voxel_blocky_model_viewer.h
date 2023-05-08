#ifndef VOXEL_BLOCKY_MODEL_VIEWER_H
#define VOXEL_BLOCKY_MODEL_VIEWER_H

#include "../../meshers/blocky/voxel_blocky_model.h"
#include "../../util/godot/classes/h_box_container.h"

ZN_GODOT_FORWARD_DECLARE(class Camera3D);
ZN_GODOT_FORWARD_DECLARE(class MeshInstance3D);

namespace zylann {

namespace voxel {

class VoxelBlockyModelViewer : public HBoxContainer {
	GDCLASS(VoxelBlockyModelViewer, HBoxContainer)
public:
	VoxelBlockyModelViewer();

	void set_model(Ref<VoxelBlockyModel> model);

private:
	void update_model();
	void rotate_model_90(Vector3i::Axis axis);
	void add_rotation_anim(Basis basis);

	void _notification(int p_what);
	void process(float delta);

	void _on_model_changed();
	// TODO Could use bind() for the axis, but in GodotCpp (GDExtension) bind() isn't implemented...
	void _on_rotate_x_button_pressed();
	void _on_rotate_y_button_pressed();
	void _on_rotate_z_button_pressed();

	static void _bind_methods();

	Ref<VoxelBlockyModel> _model;
	Camera3D *_camera = nullptr;
	MeshInstance3D *_mesh_instance = nullptr;
	MeshInstance3D *_collision_boxes_mesh_instance = nullptr;
	float _pitch = 0.f;
	float _yaw = 0.f;
	float _distance = 1.9f;
	Basis _rotation_anim_basis;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BLOCKY_MODEL_VIEWER_H
