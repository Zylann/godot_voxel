#ifndef VOXEL_BLOCKY_TYPE_LIBRARY_IDS_DIALOG_H
#define VOXEL_BLOCKY_TYPE_LIBRARY_IDS_DIALOG_H

#include "../../../meshers/blocky/types/voxel_blocky_type_library.h"
#include "../../../util/godot/classes/accept_dialog.h"

ZN_GODOT_FORWARD_DECLARE(class ItemList);

namespace zylann::voxel {

class VoxelBlockyTypeLibraryIDSDialog : public AcceptDialog {
	GDCLASS(VoxelBlockyTypeLibraryIDSDialog, AcceptDialog)
public:
	VoxelBlockyTypeLibraryIDSDialog();

	void set_library(Ref<VoxelBlockyTypeLibrary> library);

private:
	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}

	// Ref<VoxelBlockyTypeLibrary> _library;
	ItemList *_item_list = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_LIBRARY_IDS_DIALOG_H
