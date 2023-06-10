#ifndef VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_H
#define VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_H

#include "../../../meshers/blocky/types/voxel_blocky_type_library.h"
#include "../../../util/godot/classes/v_box_container.h"

ZN_GODOT_FORWARD_DECLARE(class UndoRedo);

namespace zylann::voxel {

class VoxelBlockyTypeLibraryEditor : public VBoxContainer {
	GDCLASS(VoxelBlockyTypeLibraryEditor, VBoxContainer)
public:
	VoxelBlockyTypeLibraryEditor();

	void set_library(Ref<VoxelBlockyTypeLibrary> library);

private:
	static void _bind_methods();

	Ref<VoxelBlockyTypeLibrary> _library;
	// UndoRedo *_undo_redo = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_LIBRARY_EDITOR_H
