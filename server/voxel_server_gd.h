#ifndef VOXEL_SERVER_GD_H
#define VOXEL_SERVER_GD_H

#include "core/object/class_db.h"
#include <core/object/object.h>

namespace zylann {
class ZN_ThreadedTask;
} // namespace zylann

namespace zylann::voxel::gd {

// Godot-facing singleton class.
// the real class is internal and does not need anything from Object.
class VoxelServer : public Object {
	GDCLASS(VoxelServer, Object)
public:
	static VoxelServer *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	VoxelServer();

	Dictionary get_stats() const;
	void schedule_task(Ref<ZN_ThreadedTask> task);

#ifdef TOOLS_ENABLED
	void set_editor_camera_info(Vector3 position, Vector3 direction);
	Vector3 get_editor_camera_position() const;
	Vector3 get_editor_camera_direction() const;
#endif

private:
	void _on_rendering_server_frame_post_draw();

	static void _bind_methods();

#ifdef TOOLS_ENABLED
	Vector3 _editor_camera_position;
	Vector3 _editor_camera_direction;
#endif
};

} // namespace zylann::voxel::gd

#endif // VOXEL_SERVER_GD_H
