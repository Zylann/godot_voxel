#ifndef VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H
#define VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H

#include <scene/gui/dialogs.h>

class CodeEdit;

namespace zylann::voxel {

class VoxelGraphEditorShaderDialog : public AcceptDialog {
public:
	VoxelGraphEditorShaderDialog();

	void set_shader_code(const String &code);

private:
	void _on_copy_to_clipboard_button_pressed();

	CodeEdit *_text_edit = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_SHADER_DIALOG_H
