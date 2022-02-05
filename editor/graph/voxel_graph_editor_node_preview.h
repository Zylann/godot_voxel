#ifndef VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
#define VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H

#include <scene/gui/box_container.h>

class TextureRect;

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
	TextureRect *_texture_rect = nullptr;
	Ref<ImageTexture> _texture;
	Ref<Image> _image;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_PREVIEW_H
