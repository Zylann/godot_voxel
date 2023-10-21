#include "camera_cache.h"
#include "../util/errors.h"

namespace zylann::voxel::gd {

namespace {
ObjectID g_camera_id;
Vector3 g_position;
} // namespace

Vector3 get_3d_editor_camera_position() {
	if (g_camera_id.is_null()) {
		return g_position;
	}
	Object *obj = ObjectDB::get_instance(g_camera_id);
	if (obj == nullptr) {
		g_camera_id = ObjectID();
		return g_position;
	}
	Camera3D *camera = Object::cast_to<Camera3D>(obj);
	if (camera == nullptr) {
		g_camera_id = ObjectID();
		return g_position;
	}
	g_position = camera->get_position();
	return g_position;
}

void set_3d_editor_camera_cache(Camera3D *camera) {
	ZN_ASSERT_RETURN(camera != nullptr);
	g_camera_id = camera->get_instance_id();
	g_position = camera->get_position();
}

} // namespace zylann::voxel::gd
