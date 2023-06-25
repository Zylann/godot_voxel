#ifndef ZN_AXES_3D_CONTROL_H
#define ZN_AXES_3D_CONTROL_H

#include "../../util/godot/classes/control.h"

namespace zylann {

// Displays 3D axes in a Control node using only 2D drawing.
// Similar to `ViewportRotationControl`, but much smaller to fit in smaller editors.
class ZN_Axes3DControl : public Control {
	GDCLASS(ZN_Axes3DControl, Control)
public:
	void set_basis_3d(Basis basis);

private:
	void _notification(int p_what);
	void draw();

	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}

	Basis _basis;
};

} // namespace zylann

#endif // ZN_AXES_3D_CONTROL_H
