#include "voxel_mesh_sdf_viewer.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/image_texture.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/classes/spin_box.h"
#include "../../util/godot/classes/texture_rect.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

VoxelMeshSDFViewer::VoxelMeshSDFViewer() {
	_texture_rect = memnew(TextureRect);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	_texture_rect->set_custom_minimum_size(Vector2(0, EDSCALE * 192.0));
	_texture_rect->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	add_child(_texture_rect);

	_slice_spinbox = memnew(SpinBox);
	_slice_spinbox->connect(
			"value_changed", ZN_GODOT_CALLABLE_MP(this, VoxelMeshSDFViewer, _on_slice_spinbox_value_changed));
	add_child(_slice_spinbox);
	update_slice_spinbox();

	_info_label = memnew(Label);
	_info_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	add_child(_info_label);
	update_info_label();

	Button *bake_button = memnew(Button);
	bake_button->connect("pressed", ZN_GODOT_CALLABLE_MP(this, VoxelMeshSDFViewer, _on_bake_button_pressed));
	add_child(bake_button);
	_bake_button = bake_button;
	update_bake_button();
}

void VoxelMeshSDFViewer::set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf) {
	Vector3i prev_size;

	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->disconnect("baked", ZN_GODOT_CALLABLE_MP(this, VoxelMeshSDFViewer, _on_mesh_sdf_baked));

		if (_mesh_sdf->is_baked()) {
			prev_size = _mesh_sdf->get_voxel_buffer()->get_size();
		}
	}

	if (_mesh_sdf != mesh_sdf) {
		_mesh_sdf = mesh_sdf;
	}

	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->connect("baked", ZN_GODOT_CALLABLE_MP(this, VoxelMeshSDFViewer, _on_mesh_sdf_baked));

		if (_mesh_sdf->is_baked()) {
			if (prev_size != _mesh_sdf->get_voxel_buffer()->get_size()) {
				center_slice_y();
			}
		}

		update_view();
	}

	update_bake_button();
	update_info_label();
	update_slice_spinbox();
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
	Ref<Image> image = vb->debug_print_sdf_y_slice((sdf_max - sdf_min) / 2.0, _slice_y);
	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	_texture_rect->set_texture(texture);

	// TODO Implement a raymarched view.
	// I can't do it at the moment because Godot 4 support for post-processing shaders seems broken.
}

void VoxelMeshSDFViewer::_on_bake_button_pressed() {
	ERR_FAIL_COND(_mesh_sdf.is_null());
	if (_mesh_sdf->is_baked()) {
		_size_before_baking = _mesh_sdf->get_voxel_buffer()->get_size();
	}
	_mesh_sdf->bake_async(get_tree());
	update_bake_button();
}

void VoxelMeshSDFViewer::_on_mesh_sdf_baked() {
	if (_mesh_sdf.is_valid() && _mesh_sdf->is_baked()) {
		if (_size_before_baking != _mesh_sdf->get_voxel_buffer()->get_size()) {
			center_slice_y();
		}
		update_view();
	}
	update_bake_button();
	update_info_label();
	update_slice_spinbox();
}

void VoxelMeshSDFViewer::_on_slice_spinbox_value_changed(float value) {
	if (_slice_spinbox_ignored) {
		return;
	}
	int slice_y = value;
	ZN_ASSERT_RETURN(_mesh_sdf.is_valid() && _mesh_sdf->is_baked());
	ZN_ASSERT_RETURN(slice_y >= 0 && slice_y < _mesh_sdf->get_voxel_buffer()->get_size().y);
	_slice_y = value;
	update_view();
}

void VoxelMeshSDFViewer::update_bake_button() {
	if (_mesh_sdf.is_valid()) {
		if (_mesh_sdf->is_baking()) {
			_bake_button->set_text(ZN_TTR("Baking..."));
			_bake_button->set_disabled(true);
		} else {
			_bake_button->set_text(ZN_TTR("Bake"));
			_bake_button->set_disabled(false);
		}
	} else {
		_bake_button->set_text(ZN_TTR("Bake"));
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

void VoxelMeshSDFViewer::update_slice_spinbox() {
	if (_mesh_sdf.is_null() || !_mesh_sdf->is_baked()) {
		_slice_spinbox->set_editable(false);
		return;
	}
	_slice_spinbox_ignored = true;

	_slice_spinbox->set_editable(true);
	_slice_spinbox->set_min(0);
	Ref<gd::VoxelBuffer> vb = _mesh_sdf->get_voxel_buffer();
	_slice_spinbox->set_max(vb->get_size().y);
	_slice_spinbox->set_step(1);
	_slice_spinbox->set_value(_slice_y);

	_slice_spinbox_ignored = false;
}

void VoxelMeshSDFViewer::center_slice_y() {
	ZN_ASSERT_RETURN(_mesh_sdf.is_valid() && _mesh_sdf->is_baked());
	const int size_y = _mesh_sdf->get_voxel_buffer()->get_size().y;
	const int slice_y = size_y / 2;
	_slice_y = slice_y;
}

// void VoxelMeshSDFViewer::clamp_slice_y() {
// 	ZN_ASSERT_RETURN(_mesh_sdf.is_valid() && _mesh_sdf->is_baked());
// 	const int size_y = _mesh_sdf->get_voxel_buffer()->get_size_y();
// 	const int slice_y = math::clamp(_slice_y, 0, slice_y);
// 	_slice_y = slice_y;
// }

void VoxelMeshSDFViewer::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_bake_button_pressed"), &VoxelMeshSDFViewer::_on_bake_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_mesh_sdf_baked"), &VoxelMeshSDFViewer::_on_mesh_sdf_baked);
	ClassDB::bind_method(
			D_METHOD("_on_slice_spinbox_value_changed", "value"), &VoxelMeshSDFViewer::_on_slice_spinbox_value_changed);
#endif
}

} // namespace zylann::voxel
