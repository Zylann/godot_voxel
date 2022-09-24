#include "voxel_graph_editor_node_preview.h"
#include "../../util/godot/texture_rect.h"

namespace zylann::voxel {

VoxelGraphEditorNodePreview::VoxelGraphEditorNodePreview() {
	_image.instantiate();
	_image->create(RESOLUTION, RESOLUTION, false, Image::FORMAT_L8);
	_image->fill(Color(0.5, 0.5, 0.5));
	_texture = ImageTexture::create_from_image(_image);
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
	_texture->update(_image);
}

} // namespace zylann::voxel
