#ifndef VOXEL_BLOCKY_MODEL_VIEWER_H
#define VOXEL_BLOCKY_MODEL_VIEWER_H

#include "../../meshers/blocky/voxel_blocky_model.h"
#include "../../util/godot/classes/h_box_container.h"

ZN_GODOT_FORWARD_DECLARE(class Camera3D);
ZN_GODOT_FORWARD_DECLARE(class MeshInstance3D);

namespace zylann {

class ZN_Axes3DControl;

namespace voxel {

class VoxelBlockyModelViewer : public HBoxContainer {
	GDCLASS(VoxelBlockyModelViewer, HBoxContainer)
public:
	VoxelBlockyModelViewer();

	void set_model(Ref<VoxelBlockyModel> model);

private:
#if defined(ZN_GODOT)
	void gui_input(const Ref<InputEvent> &p_event) override;
#elif defined(ZN_GODOT_EXTENSION)
	void _gui_input(const Ref<InputEvent> &p_event) override;
#endif
	void update_model();
	void update_camera();
	void rotate_model_90(Vector3i::Axis axis);
	void add_rotation_anim(Basis basis);

	void _notification(int p_what);
	void process(float delta);

	void _on_model_changed();
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
	ZN_Axes3DControl *_axes_3d_control = nullptr;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BLOCKY_MODEL_VIEWER_H
