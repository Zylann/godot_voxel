#ifndef VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
#define VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H

#include "../../util/godot/image.h"
#include "../../util/godot/image_texture.h"
#include "../../util/godot/v_box_container.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class TextureRect)

namespace zylann::voxel {

// Shows a 2D slice of the 3D set of values coming from an output port
class VoxelGraphEditorNodePreview : public VBoxContainer {
	GDCLASS(VoxelGraphEditorNodePreview, VBoxContainer)
public:
	static const int RESOLUTION = 128;

	VoxelGraphEditorNodePreview();

	Ref<Image> get_image() const;
	void update_texture();

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}

	TextureRect *_texture_rect = nullptr;
	Ref<ImageTexture> _texture;
	Ref<Image> _image;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
