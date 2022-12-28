#ifndef VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H
#define VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H

#include "../../util/godot/classes/accept_dialog.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class CodeEdit)

namespace zylann::voxel {

class VoxelGraphEditorShaderDialog : public AcceptDialog {
	GDCLASS(VoxelGraphEditorShaderDialog, AcceptDialog)
public:
	VoxelGraphEditorShaderDialog();

	void set_shader_code(const String &code);

private:
	void _on_copy_to_clipboard_button_pressed();

	static void _bind_methods();

	CodeEdit *_text_edit = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H
