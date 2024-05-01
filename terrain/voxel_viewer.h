#ifndef VOXEL_VIEWER_H
#define VOXEL_VIEWER_H

#include "../engine/ids.h"
#include "../util/godot/classes/node_3d.h"
#include "../util/math/vector2f.h"

namespace zylann::voxel {

// Triggers loading of voxel nodes around its position. Voxels will update in priority closer to viewers.
// Usually added as child of the player's camera.
class VoxelViewer : public Node3D {
	GDCLASS(VoxelViewer, Node3D)
public:
	VoxelViewer();

	// Distance in world space units
	void set_view_distance(unsigned int distance);
	unsigned int get_view_distance() const;
	// TODO Collision distance

	void set_view_distance_vertical_ratio(float p_ratio);
	float get_view_distance_vertical_ratio() const;

	// TODO Have an option to run in editor, could be useful for testing?

	void set_requires_visuals(bool enabled);
	bool is_requiring_visuals() const;

	void set_requires_collisions(bool enabled);
	bool is_requiring_collisions() const;

	void set_requires_data_block_notifications(bool enabled);
	bool is_requiring_data_block_notifications() const;

	void set_network_peer_id(int id);
	int get_network_peer_id() const;

	void set_enabled_in_editor(bool enable);
	bool is_enabled_in_editor() const;

protected:
	void _notification(int p_what);

private:
	static void _bind_methods();

	void sync_all_parameters();
	void sync_view_distances();

	bool is_active() const;

	ViewerID _viewer_id;
	unsigned int _view_distance = 128;
	float _view_distance_vertical_ratio = 1.f;
	bool _requires_visuals = true;
	bool _requires_collisions = true;
	bool _requires_data_block_notifications = false;
	bool _enabled_in_editor = false;
	int _network_peer_id = -1;
};

} // namespace zylann::voxel

#endif // VOXEL_VIEWER_H
