#ifndef VOXEL_DEBUG_H
#define VOXEL_DEBUG_H

#include "../util/godot/direct_multimesh_instance.h"
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
	ID_WHITE,
	ID_COUNT
};

Ref<Mesh> get_wirecube(ColorID id);
void free_resources();

class DebugMultiMeshRenderer {
public:
	DebugMultiMeshRenderer();

	void set_world(World *world);
	void begin();
	void draw_box(const Transform &t, Color8 color);
	void end();
	void clear();

private:
	std::vector<DirectMultiMeshInstance::TransformAndColor8> _items;
	Ref<MultiMesh> _multimesh;
	DirectMultiMeshInstance _multimesh_instance;
	World *_world = nullptr;
	bool _inside_block = false;
	PoolRealArray _bulk_array;
	Ref<SpatialMaterial> _material;
};

class DebugRendererItem;

class DebugRenderer {
public:
	~DebugRenderer();

	void set_world(World *world);

	void begin();
	void draw_box(const Transform &t, ColorID color);
	void draw_box_mm(const Transform &t, Color8 color);
	void end();
	void clear();

private:
	std::vector<DebugRendererItem *> _items;
	unsigned int _current = 0;
	bool _inside_block = false;
	World *_world = nullptr;
	DebugMultiMeshRenderer _mm_renderer;
};

} // namespace VoxelDebug

#endif // VOXEL_DEBUG_H
