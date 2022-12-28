#ifndef VOXEL_GRAPH_EDITOR_WINDOW_H
#define VOXEL_GRAPH_EDITOR_WINDOW_H

#include "../../util/godot/classes/accept_dialog.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

// TODO It would be really nice if we were not forced to use an AcceptDialog for making a window.
// AcceptDialog adds stuff I don't need, but Window is too low level.
class VoxelGraphEditorWindow : public AcceptDialog {
	GDCLASS(VoxelGraphEditorWindow, AcceptDialog)
public:
	VoxelGraphEditorWindow() {
		set_exclusive(false);
		set_close_on_escape(false);
		get_ok_button()->hide();
		set_min_size(Vector2(600, 300) * EDSCALE);
		// I want the window to remain on top of the editor if the editor is given focus. `always_on_top` is the only
		// property allowing that, but it requires `transient` to be `false`. Without `transient`, the window is no
		// longer considered a child and won't give back focus to the editor when closed.
		// So for now, the window will get hidden behind the editor if you click on the editor.
		// you'll have to suffer moving popped out windows out of the editor area if you want to see them both...
		//set_flag(Window::FLAG_ALWAYS_ON_TOP, true);
	}

	// void _notification(int p_what) {
	// 	switch (p_what) {
	// 		case NOTIFICATION_WM_CLOSE_REQUEST:
	// 			call_deferred(SNAME("hide"));
	// 			break;
	// 	}
	// }

	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_WINDOW_H
