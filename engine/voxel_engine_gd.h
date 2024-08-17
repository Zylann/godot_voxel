#ifndef VOXEL_ENGINE_GD_H
#define VOXEL_ENGINE_GD_H

#include "voxel_engine.h"

namespace zylann {
class ZN_ThreadedTask;
} // namespace zylann

namespace zylann::voxel::godot {

// Godot-facing singleton class.
// the real class is internal and does not need anything from Object.
class VoxelEngine : public Object {
	GDCLASS(VoxelEngine, Object)
public:
	static VoxelEngine *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	struct Config {
		zylann::voxel::VoxelEngine::Config inner;
		bool ownership_checks;
	};

	static Config get_config_from_godot();

	VoxelEngine();

	int get_version_major() const;
	int get_version_minor() const;
	int get_version_patch() const;

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

} // namespace zylann::voxel::godot

#endif // VOXEL_ENGINE_GD_H
