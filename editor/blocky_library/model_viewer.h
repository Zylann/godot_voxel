#ifndef ZN_GODOT_MODEL_VIEWER_H
#define ZN_GODOT_MODEL_VIEWER_H

#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/control.h"

// Required in header for GDExtension builds, due to how virtual methods are implemented
#include "../../util/godot/classes/input_event.h"

#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Camera3D);
ZN_GODOT_FORWARD_DECLARE(class SubViewport);

namespace zylann {

class ZN_Axes3DControl;

// Basic SubViewport embedded in a Control for viewing 3D stuff.
// Implements camera controls orbitting around the origin.
// Godot has `MeshEditor` but it is specialized for Mesh resources without access to the hierarchy.
class ZN_ModelViewer : public Control {
	GDCLASS(ZN_ModelViewer, Control)
public:
	ZN_ModelViewer();

	void set_camera_distance(float d);
	void set_camera_target_position(const Vector3 pos);
	void set_zoom_level(const unsigned int i);
	void set_zoom_max_level(const unsigned int max);
	void set_zoom_distance_range(const float min_distance, const float max_distance);

	// Stuff to view can be instanced under this node
	Node *get_viewer_root_node() const;

#if defined(ZN_GODOT)
	void gui_input(const Ref<InputEvent> &p_event) override;
#elif defined(ZN_GODOT_EXTENSION)
	void _gui_input(const Ref<InputEvent> &p_event) override;
#endif

private:
	void update_camera();

	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}

	Camera3D *_camera = nullptr;
	Vector3 _target_position = Vector3(0.5, 0.5, 0.5);
	float _pitch = 0.f;
	float _yaw = 0.f;
	float _distance = 1.9f;

	float _zoom_min_distance = 0.f;
	float _zoom_max_distance = 1.9f; // Distance at zoom level 0
	float _zoom_factor = 1.5f;
	uint8_t _zoom_level = 0; // The higher, the more zoomed in.
	uint8_t _zoom_max_level = 0;

	ZN_Axes3DControl *_axes_3d_control = nullptr;
	SubViewport *_viewport;
};

} // namespace zylann

#endif // ZN_GODOT_MODEL_VIEWER_H
