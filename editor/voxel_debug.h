#ifndef VOXEL_DEBUG_H
#define VOXEL_DEBUG_H

#include <core/reference.h>
#include <vector>

class Mesh;
class DirectMeshInstance;
class World;

namespace VoxelDebug {

enum ColorID {
	ID_VOXEL_BOUNDS = 0,
	ID_OCTREE_BOUNDS,
	ID_VOXEL_GRAPH_DEBUG_BOUNDS,
	ID_COUNT
};

Ref<Mesh> get_wirecube(ColorID id);
void free_resources();

class DebugRendererItem;

class DebugRenderer {
public:
	~DebugRenderer();

	void set_world(World *world);

	void begin();
	void draw_box(Transform t, ColorID color);
	void end();
	void clear();

private:
	std::vector<DebugRendererItem *> _items;
	unsigned int _current = 0;
	bool _inside_block = false;
	World *_world = nullptr;
};

} // namespace VoxelDebug

#endif // VOXEL_DEBUG_H
