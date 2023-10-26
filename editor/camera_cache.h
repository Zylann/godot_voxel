#ifndef VOXEL_EDITOR_CAMERA_CACHE_H
#define VOXEL_EDITOR_CAMERA_CACHE_H

#include "../util/godot/classes/camera_3d.h"
#include "../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Camera3D);

namespace zylann::voxel::gd {

// This is a workaround.
// In the Godot Editor, `get_viewport()->get_camera_3d()` always returns `nullptr`, so if a node in the edited scene
// needs it, it can't access it. Besides, the editor can have more than one camera (rendering the same scene, or a
// different scene!). EditorPlugins can access such cameras alongside spatial input events, but this is very impractical
// to get from the edited node, and it also requires the node to be selected. So instead, we can cache the camera.

Vector3 get_3d_editor_camera_position();
void set_3d_editor_camera_cache(Camera3D *camera);

} // namespace zylann::voxel::gd

#endif // VOXEL_EDITOR_CAMERA_CACHE_H
