#include "editor_file_dialog.h"
#include "../core/version.h"
#include "../editor_scale.h"

namespace zylann::godot {

void popup_file_dialog(EditorFileDialog &dialog) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 3
	dialog.popup_centered_clamped(Size2(1050, 700) * EDSCALE, 0.8);
#else
	dialog.popup_file_dialog();
#endif
}

} // namespace zylann::godot
