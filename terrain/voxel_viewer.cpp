#include "voxel_viewer.h"
#include "../engine/voxel_engine.h"
#include "../util/godot/classes/engine.h"
#include "../util/godot/classes/node.h"
#include "../util/math/conv.h"
#include "../util/string/format.h"

namespace zylann::voxel {

VoxelViewer::VoxelViewer() {
	set_notify_transform(!Engine::get_singleton()->is_editor_hint());
}

void VoxelViewer::set_view_distance(unsigned int distance) {
	_view_distance = distance;
	if (is_active()) {
		sync_view_distances();
	}
}

unsigned int VoxelViewer::get_view_distance() const {
	return _view_distance;
}

void VoxelViewer::set_view_distance_vertical_ratio(float p_ratio) {
	_view_distance_vertical_ratio = math::max(p_ratio, 0.01f);
	if (is_active()) {
		sync_view_distances();
	}
}

float VoxelViewer::get_view_distance_vertical_ratio() const {
	return _view_distance_vertical_ratio;
}

void VoxelViewer::set_requires_visuals(bool enabled) {
	_requires_visuals = enabled;
	if (is_active()) {
		VoxelEngine::get_singleton().set_viewer_requires_visuals(_viewer_id, enabled);
	}
}

bool VoxelViewer::is_requiring_visuals() const {
	return _requires_visuals;
}

void VoxelViewer::set_requires_collisions(bool enabled) {
	_requires_collisions = enabled;
	if (is_active()) {
		VoxelEngine::get_singleton().set_viewer_requires_collisions(_viewer_id, enabled);
	}
}

bool VoxelViewer::is_requiring_collisions() const {
	return _requires_collisions;
}

void VoxelViewer::set_requires_data_block_notifications(bool enabled) {
	_requires_data_block_notifications = enabled;
	if (is_active()) {
		VoxelEngine::get_singleton().set_viewer_requires_data_block_notifications(_viewer_id, enabled);
	}
}

bool VoxelViewer::is_requiring_data_block_notifications() const {
	return _requires_data_block_notifications;
}

void VoxelViewer::set_network_peer_id(int id) {
	_network_peer_id = id;
	if (is_active()) {
		VoxelEngine::get_singleton().set_viewer_network_peer_id(_viewer_id, id);
	}
}

int VoxelViewer::get_network_peer_id() const {
	return _network_peer_id;
}

void VoxelViewer::set_enabled_in_editor(bool enable) {
	if (_enabled_in_editor == enable) {
		return;
	}

	_enabled_in_editor = enable;

#ifdef TOOLS_ENABLED
	// This setting only has an effect when in the editor.
	// Note, `is_editor_hint` is not supposed to change during execution.
	if (Engine::get_singleton()->is_editor_hint()) {
		set_notify_transform(_enabled_in_editor);

		if (is_inside_tree()) {
			if (_enabled_in_editor) {
				_viewer_id = VoxelEngine::get_singleton().add_viewer();
				sync_all_parameters();

			} else {
				VoxelEngine::get_singleton().remove_viewer(_viewer_id);
			}
		}
	}
#endif
}

bool VoxelViewer::is_enabled_in_editor() const {
	return _enabled_in_editor;
}

void VoxelViewer::sync_view_distances() {
	VoxelEngine::Viewer::Distances distances;
	distances.horizontal = _view_distance;
	distances.vertical = _view_distance;

	if (!Math::is_equal_approx(_view_distance_vertical_ratio, 1.f)) {
		distances.vertical =
				math::max(1, static_cast<int>(static_cast<float>(_view_distance) * _view_distance_vertical_ratio));
	}

	VoxelEngine::get_singleton().set_viewer_distances(_viewer_id, distances);
}

void VoxelViewer::sync_all_parameters() {
	sync_view_distances();
	VoxelEngine &ve = VoxelEngine::get_singleton();
	ve.set_viewer_requires_visuals(_viewer_id, _requires_visuals);
	ve.set_viewer_requires_collisions(_viewer_id, _requires_collisions);
	ve.set_viewer_requires_data_block_notifications(_viewer_id, _requires_data_block_notifications);
	ve.set_viewer_network_peer_id(_viewer_id, _network_peer_id);
	const Vector3 pos = get_global_transform().origin;
	ve.set_viewer_position(_viewer_id, pos);
}

void VoxelViewer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint() || _enabled_in_editor) {
				if (!_pending_deferred_unregistration) {
					_viewer_id = VoxelEngine::get_singleton().add_viewer();
				} else {
					ZN_ASSERT_RETURN(VoxelEngine::get_singleton().viewer_exists(_viewer_id));
				}
				sync_all_parameters();
				// VoxelEngine::get_singleton().sync_viewers_task_priority_data();
			}
		} break;

		case NOTIFICATION_EXIT_TREE:
			if (!Engine::get_singleton()->is_editor_hint() || _enabled_in_editor) {
				if (!_pending_deferred_unregistration) {
					// When users reparent nodes, Godot triggers an EXIT_TREE, followed by a separate ENTER_TREE. So
					// reparenting viewers is indistinguishable from solely adding, or removing a node. If we unregister
					// right now, it leads to unexpected issues with terrain pairing, like the whole area reloading
					// because the viewer got unregistered and re-registered. Since reparenting usually takes place with
					// `remove_child` immediately followed by `add_child`, we can workaround the issue by deferring
					// unregistration. Unfortunately, we then have to workaround the case the node gets deleted
					// (`remove_child` followed by `free`, or `queue_free`), otherwise the deferred call could end
					// badly.
					_pending_deferred_unregistration = true;
					callable_mp_static(&VoxelViewer::unregister_deferred_callback)
							.bind(get_instance_id(), Vector2i(_viewer_id.index, _viewer_id.version.value))
							.call_deferred();
					// VoxelEngine::get_singleton().remove_viewer(_viewer_id);
				}
			}
			break;

		case NOTIFICATION_TRANSFORM_CHANGED:
			if (is_active()) {
				const Vector3 pos = get_global_transform().origin;
				VoxelEngine::get_singleton().set_viewer_position(_viewer_id, pos);
			}
			break;

		default:
			break;
	}
}

// TODO GDX: using `callable_mp_static` on a function taking an `ObjectID` doesn't compile with GodotCpp (but does in
// Godot core)
// void VoxelViewer::unregister_deferred_callback(const ObjectID viewer_node_id, const Vector2i encoded_viewer_id) {
void VoxelViewer::unregister_deferred_callback(const int64_t viewer_node_id, const Vector2i encoded_viewer_id) {
	Object *obj = ObjectDB::get_instance(ObjectID(viewer_node_id));
	VoxelViewer *viewer = Object::cast_to<VoxelViewer>(obj);
	if (viewer != nullptr) {
		viewer->_pending_deferred_unregistration = false;
		if (viewer->is_inside_tree()) {
			// The viewer is still in tree, so we can assume it was only reparented, so don't unregister it
			// TODO When multi-world becomes supported, we have to re-check if the viewer has changed world...
			return;
		}
	}
	// The node got removed and not added back, or was destroyed
	ViewerID viewer_id;
	viewer_id.index = encoded_viewer_id.x;
	viewer_id.version.value = encoded_viewer_id.y;
	ZN_ASSERT_RETURN(VoxelEngine::get_singleton().viewer_exists(viewer_id));
	VoxelEngine::get_singleton().remove_viewer(viewer_id);
}

bool VoxelViewer::is_active() const {
	return is_inside_tree() && (!Engine::get_singleton()->is_editor_hint() || _enabled_in_editor);
}

void VoxelViewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_view_distance", "distance"), &VoxelViewer::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelViewer::get_view_distance);

	ClassDB::bind_method(
			D_METHOD("set_view_distance_vertical_ratio", "ratio"), &VoxelViewer::set_view_distance_vertical_ratio
	);
	ClassDB::bind_method(D_METHOD("get_view_distance_vertical_ratio"), &VoxelViewer::get_view_distance_vertical_ratio);

	ClassDB::bind_method(D_METHOD("set_requires_visuals", "enabled"), &VoxelViewer::set_requires_visuals);
	ClassDB::bind_method(D_METHOD("is_requiring_visuals"), &VoxelViewer::is_requiring_visuals);

	ClassDB::bind_method(D_METHOD("set_requires_collisions", "enabled"), &VoxelViewer::set_requires_collisions);
	ClassDB::bind_method(D_METHOD("is_requiring_collisions"), &VoxelViewer::is_requiring_collisions);

	ClassDB::bind_method(
			D_METHOD("set_requires_data_block_notifications", "enabled"),
			&VoxelViewer::set_requires_data_block_notifications
	);
	ClassDB::bind_method(
			D_METHOD("is_requiring_data_block_notifications"), &VoxelViewer::is_requiring_data_block_notifications
	);

	ClassDB::bind_method(D_METHOD("set_network_peer_id", "id"), &VoxelViewer::set_network_peer_id);
	ClassDB::bind_method(D_METHOD("get_network_peer_id"), &VoxelViewer::get_network_peer_id);

	ClassDB::bind_method(D_METHOD("set_enabled_in_editor", "enabled"), &VoxelViewer::set_enabled_in_editor);
	ClassDB::bind_method(D_METHOD("is_enabled_in_editor"), &VoxelViewer::is_enabled_in_editor);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "view_distance_vertical_ratio"),
			"set_view_distance_vertical_ratio",
			"get_view_distance_vertical_ratio"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "requires_visuals"), "set_requires_visuals", "is_requiring_visuals");
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "requires_collisions"), "set_requires_collisions", "is_requiring_collisions"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "requires_data_block_notifications"),
			"set_requires_data_block_notifications",
			"is_requiring_data_block_notifications"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled_in_editor"), "set_enabled_in_editor", "is_enabled_in_editor");
}

} // namespace zylann::voxel
