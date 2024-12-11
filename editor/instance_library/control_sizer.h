#ifndef ZN_GODOT_CONTROL_SIZER_H
#define ZN_GODOT_CONTROL_SIZER_H

#include "../../util/godot/classes/control.h"
#include "../../util/godot/object_weak_ref.h"

namespace zylann {

// Implements similar logic as the middle resizing handle of SplitContainer, but works on a target control instead
class ZN_ControlSizer : public Control {
	GDCLASS(ZN_ControlSizer, Control)
public:
	ZN_ControlSizer();

	void set_target_control(Control *control);

#ifdef ZN_GODOT
	void gui_input(const Ref<InputEvent> &p_event) override;
#elif defined(ZN_GODOT_EXTENSION)
	void _gui_input(const Ref<InputEvent> &p_event) override;
#endif

private:
	static void _bind_methods();

	void _notification(int p_what);

	void cache_theme();

	zylann::godot::ObjectWeakRef<Control> _target_control;
	bool _dragging = false;
	bool _mouse_inside = false;
	float _min_size = 10.0;
	float _max_size = 1000.0;
	Ref<Texture2D> _hover_icon;
};

} // namespace zylann

#endif // ZN_GODOT_CONTROL_SIZER_H
