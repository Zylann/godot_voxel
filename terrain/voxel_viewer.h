#ifndef VOXEL_VIEWER_H
#define VOXEL_VIEWER_H

#include <scene/3d/spatial.h>

// Triggers loading of voxel nodes around its position. Voxels will update in priority closer to viewers.
// Usually added as child of the player's camera.
class VoxelViewer : public Spatial {
	GDCLASS(VoxelViewer, Spatial)
public:
	VoxelViewer();

	// Distance in world space units
	void set_view_distance(unsigned int distance);
	unsigned int get_view_distance() const;
	// TODO Collision distance

	void set_requires_visuals(bool enabled);
	bool is_requiring_visuals() const;

	void set_requires_collisions(bool enabled);
	bool is_requiring_collisions() const;

protected:
	void _notification(int p_what);

private:
	static void _bind_methods();

	void _process();
	bool is_active() const;

	uint32_t _viewer_id = 0;
	unsigned int _view_distance = 128;
	bool _requires_visuals = true;
	bool _requires_collisions = true;
};

#endif // VOXEL_VIEWER_H
