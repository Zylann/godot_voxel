#include "voxel_graph_editor_node_preview.h"

#include <scene/gui/texture_rect.h>

namespace zylann::voxel {

VoxelGraphEditorNodePreview::VoxelGraphEditorNodePreview() {
	_image.instantiate();
	_image->create(RESOLUTION, RESOLUTION, false, Image::FORMAT_L8);
	_texture.instantiate();
	update_texture();
	_texture_rect = memnew(TextureRect);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
	_texture_rect->set_custom_minimum_size(Vector2(RESOLUTION, RESOLUTION));
	_texture_rect->set_texture(_texture);
	_texture_rect->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	add_child(_texture_rect);
}

Ref<Image> VoxelGraphEditorNodePreview::get_image() const {
	return _image;
}

void VoxelGraphEditorNodePreview::update_texture() {
	_texture->create_from_image(_image);
}

} // namespace zylann::voxel
