#include "model_viewer.h"
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/directional_light_3d.h"
#include "../../util/godot/classes/input_event_mouse_motion.h"
#include "../../util/godot/classes/sub_viewport.h"
#include "../../util/godot/classes/sub_viewport_container.h"
#include "../../util/godot/classes/world_3d.h"
#include "../../util/godot/core/mouse_button.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/math/funcs.h"
#include "axes_3d_control.h"
#include <limits>

namespace zylann {

ZN_ModelViewer::ZN_ModelViewer() {
	_pitch = math::deg_to_rad(-28.f);
	_yaw = math::deg_to_rad(-37.f);

	Ref<World3D> world;
	world.instantiate();

	_viewport = memnew(SubViewport);
	_viewport->set_world_3d(world);
	_viewport->set_use_own_world_3d(true);

	_camera = memnew(Camera3D);
	_camera->set_fov(50.f);
	_viewport->add_child(_camera);

	DirectionalLight3D *light = memnew(DirectionalLight3D);
	light->set_transform(Transform3D(Basis::looking_at(Vector3(1, -1, -2)), Vector3()));
	_camera->add_child(light);

	const float editor_scale = EDSCALE;

	// SubViewportContainer inherits Container for some reason, so it is not possible to add child controls using
	// anchors and margins. Therefore, the container is child of our control, rather than being the container itself.
	SubViewportContainer *viewport_container = memnew(SubViewportContainer);
	viewport_container->add_child(_viewport);
	viewport_container->set_stretch(true);
	viewport_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(viewport_container);

	_axes_3d_control = memnew(ZN_Axes3DControl);
	_axes_3d_control->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
	//_axes_3d_control->set_size(editor_scale * Vector2(32, 32));
	_axes_3d_control->set_offset(SIDE_LEFT, 0);
	_axes_3d_control->set_offset(SIDE_RIGHT, 32 * editor_scale);
	_axes_3d_control->set_offset(SIDE_TOP, -32 * editor_scale);
	_axes_3d_control->set_offset(SIDE_BOTTOM, 0);
	_axes_3d_control->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(_axes_3d_control);

	update_camera();
}

void ZN_ModelViewer::set_camera_distance(float d) {
	_distance = d;
	update_camera();
}

void ZN_ModelViewer::set_camera_target_position(const Vector3 pos) {
	_target_position = pos;
	update_camera();
}

void ZN_ModelViewer::set_zoom_level(const unsigned int i) {
	_zoom_level = math::clamp<unsigned int>(i, 0, _zoom_max_level);
	update_camera();
}

void ZN_ModelViewer::set_zoom_max_level(const unsigned int max) {
	ZN_ASSERT_RETURN(max <= std::numeric_limits<uint8_t>::max());
	_zoom_max_level = max;
	update_camera();
}

void ZN_ModelViewer::set_zoom_distance_range(const float min_distance, const float max_distance) {
	_zoom_min_distance = min_distance;
	_zoom_max_distance = max_distance;
	update_camera();
}

void ZN_ModelViewer::set_ctrl_to_zoom(const bool ctrl_to_zoom) {
	_ctrl_to_zoom = ctrl_to_zoom;
}

Node *ZN_ModelViewer::get_viewer_root_node() const {
	return _viewport;
}

#if defined(ZN_GODOT)
void ZN_ModelViewer::gui_input(const Ref<InputEvent> &p_event) {
#elif defined(ZN_GODOT_EXTENSION)
void ZN_ModelViewer::_gui_input(const Ref<InputEvent> &p_event) {
#endif
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (mm->get_button_mask().has_flag(ZN_GODOT_MouseButtonMask_MIDDLE)) {
			const float sensitivity = 0.01f;
			const Vector2 delta = mm->get_relative() * sensitivity;
			_pitch -= delta.y;
			_yaw -= delta.x;
			_pitch = math::clamp(_pitch, -math::PI<float> / 2.f, math::PI<float> / 2.f);
			_yaw = Math::wrapf(_yaw, -math::PI<float>, math::PI<float>);
			update_camera();
		}
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			if (!_ctrl_to_zoom || mb->is_ctrl_pressed()) {
				switch (mb->get_button_index()) {
					case ZN_GODOT_MouseButton_WHEEL_UP:
						if (_zoom_max_level > 0 && _zoom_level < _zoom_max_level - 1) {
							++_zoom_level;
							update_camera();
						}
						// Prevent scrolling of inspector
						get_viewport()->set_input_as_handled();
						break;

					case ZN_GODOT_MouseButton_WHEEL_DOWN:
						if (_zoom_level > 0) {
							--_zoom_level;
							update_camera();
						}
						// Prevent scrolling of inspector
						get_viewport()->set_input_as_handled();
						break;
				}
			}
		}
	}
}

void ZN_ModelViewer::update_camera() {
	if (_zoom_max_level > 0) {
		_distance = _zoom_min_distance +
				(_zoom_max_distance - _zoom_min_distance) * math::pow(_zoom_factor, -static_cast<float>(_zoom_level));
	}

	Basis basis;
	basis.set_euler(Vector3(_pitch, _yaw, 0));
	const Vector3 forward = -basis.get_column(Vector3::AXIS_Z);

	_camera->set_transform(Transform3D(basis, _target_position - _distance * forward));

	_axes_3d_control->set_basis_3d(basis.inverse());
}

} // namespace zylann
