#include "voxel_mesh_sdf_viewer.h"

#include <editor/editor_scale.h>
#include <scene/gui/button.h>
#include <scene/gui/label.h>
#include <scene/gui/texture_rect.h>

namespace zylann::voxel {

VoxelMeshSDFViewer::VoxelMeshSDFViewer() {
	_texture_rect = memnew(TextureRect);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	_texture_rect->set_custom_minimum_size(Vector2(0, EDSCALE * 128.0));
	_texture_rect->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	add_child(_texture_rect);

	_info_label = memnew(Label);
	_info_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	add_child(_info_label);
	update_info_label();

	Button *bake_button = memnew(Button);
	bake_button->connect("pressed", callable_mp(this, &VoxelMeshSDFViewer::_on_bake_button_pressed));
	add_child(bake_button);
	_bake_button = bake_button;
	update_bake_button();
}

void VoxelMeshSDFViewer::set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf) {
	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->disconnect("baked", callable_mp(this, &VoxelMeshSDFViewer::_on_mesh_sdf_baked));
	}

	if (_mesh_sdf != mesh_sdf) {
		_mesh_sdf = mesh_sdf;
		update_view();
	}

	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->connect("baked", callable_mp(this, &VoxelMeshSDFViewer::_on_mesh_sdf_baked));
	}

	update_bake_button();
	update_info_label();
}

void VoxelMeshSDFViewer::update_view() {
	ERR_FAIL_COND(_mesh_sdf.is_null());
	if (!_mesh_sdf->is_baked()) {
		_texture_rect->set_texture(Ref<Texture>());
		return;
	}
	Ref<gd::VoxelBuffer> vb = _mesh_sdf->get_voxel_buffer();
	float sdf_min;
	float sdf_max;
	vb->get_buffer().get_range_f(sdf_min, sdf_max, VoxelBufferInternal::CHANNEL_SDF);
	Ref<Image> image = vb->debug_print_sdf_y_slice((sdf_max - sdf_min) / 2.0, vb->get_size_y() / 2);
	Ref<ImageTexture> texture;
	texture.instantiate();
	texture->create_from_image(image);
	_texture_rect->set_texture(texture);

	// TODO Implement a raymarched view.
	// I can't do it at the moment because Godot 4 support for post-processing shaders seems broken.
}

void VoxelMeshSDFViewer::_on_bake_button_pressed() {
	ERR_FAIL_COND(_mesh_sdf.is_null());
	_mesh_sdf->bake_async(get_tree());
	update_bake_button();
}

void VoxelMeshSDFViewer::_on_mesh_sdf_baked() {
	if (_mesh_sdf.is_valid()) {
		update_view();
	}
	update_bake_button();
	update_info_label();
}

void VoxelMeshSDFViewer::update_bake_button() {
	if (_mesh_sdf.is_valid()) {
		if (_mesh_sdf->is_baking()) {
			_bake_button->set_text(TTR("Baking..."));
			_bake_button->set_disabled(true);
		} else {
			_bake_button->set_text(TTR("Bake"));
			_bake_button->set_disabled(false);
		}
	} else {
		_bake_button->set_text(TTR("Bake"));
		_bake_button->set_disabled(true);
	}
}

void VoxelMeshSDFViewer::update_info_label() {
	if (_mesh_sdf.is_valid() && _mesh_sdf->is_baked()) {
		const Vector3i size = _mesh_sdf->get_voxel_buffer()->get_size();
		_info_label->set_text(String("{0}x{1}x{2}").format(varray(size.x, size.y, size.z)));
	} else {
		_info_label->set_text("<Empty>");
	}
}

} // namespace zylann::voxel
