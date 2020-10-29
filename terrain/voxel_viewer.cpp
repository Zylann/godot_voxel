#include "voxel_viewer.h"
#include "../server/voxel_server.h"
#include <core/engine.h>

VoxelViewer::VoxelViewer() {
	set_notify_transform(!Engine::get_singleton()->is_editor_hint());
}

void VoxelViewer::set_view_distance(unsigned int distance) {
	_view_distance = distance;
	if (is_active()) {
		VoxelServer::get_singleton()->set_viewer_distance(_viewer_id, distance);
	}
}

unsigned int VoxelViewer::get_view_distance() const {
	return _view_distance;
}

void VoxelViewer::set_requires_visuals(bool enabled) {
	_requires_visuals = enabled;
	if (is_active()) {
		VoxelServer::get_singleton()->set_viewer_requires_visuals(_viewer_id, enabled);
	}
}

bool VoxelViewer::is_requiring_visuals() const {
	return _requires_visuals;
}

void VoxelViewer::set_requires_collisions(bool enabled) {
	_requires_collisions = enabled;
	if (is_active()) {
		VoxelServer::get_singleton()->set_viewer_requires_collisions(_viewer_id, enabled);
	}
}

bool VoxelViewer::is_requiring_collisions() const {
	return _requires_collisions;
}

void VoxelViewer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				_viewer_id = VoxelServer::get_singleton()->add_viewer();
				VoxelServer::get_singleton()->set_viewer_distance(_viewer_id, _view_distance);
				VoxelServer::get_singleton()->set_viewer_requires_visuals(_viewer_id, _requires_visuals);
				VoxelServer::get_singleton()->set_viewer_requires_collisions(_viewer_id, _requires_collisions);
				const Vector3 pos = get_global_transform().origin;
				VoxelServer::get_singleton()->set_viewer_position(_viewer_id, pos);
			}
		} break;

		case NOTIFICATION_EXIT_TREE:
			if (!Engine::get_singleton()->is_editor_hint()) {
				VoxelServer::get_singleton()->remove_viewer(_viewer_id);
			}
			break;

		case NOTIFICATION_TRANSFORM_CHANGED:
			if (is_active()) {
				const Vector3 pos = get_global_transform().origin;
				VoxelServer::get_singleton()->set_viewer_position(_viewer_id, pos);
			}
			break;

		default:
			break;
	}
}

bool VoxelViewer::is_active() const {
	return is_inside_tree() && !Engine::get_singleton()->is_editor_hint();
}

void VoxelViewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_view_distance", "distance"), &VoxelViewer::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelViewer::get_view_distance);

	ClassDB::bind_method(D_METHOD("set_requires_visuals", "enabled"), &VoxelViewer::set_requires_visuals);
	ClassDB::bind_method(D_METHOD("is_requiring_visuals"), &VoxelViewer::is_requiring_visuals);

	ClassDB::bind_method(D_METHOD("set_requires_collisions", "enabled"), &VoxelViewer::set_requires_collisions);
	ClassDB::bind_method(D_METHOD("is_requiring_collisions"), &VoxelViewer::is_requiring_collisions);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "requires_visuals"), "set_requires_visuals", "is_requiring_visuals");
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "requires_collisions"), "set_requires_collisions", "is_requiring_collisions");
}
