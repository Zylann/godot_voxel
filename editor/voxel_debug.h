#ifndef VOXEL_DEBUG_H
#define VOXEL_DEBUG_H

#include "../util/godot/direct_multimesh_instance.h"
#include <core/object/ref_counted.h>
#include <vector>

class Mesh;
class DirectMeshInstance3D;
class World3D;

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

	void set_world(World3D *world);
	void begin();
	void draw_box(const Transform3D &t, Color8 color);
	void end();
	void clear();

private:
	std::vector<DirectMultiMeshInstance3D::Transform3DAndColor8> _items;
	Ref<MultiMesh> _multimesh;
	DirectMultiMeshInstance3D _multimesh_instance;
	World3D *_world = nullptr;
	bool _inside_block = false;
	PackedFloat32Array _bulk_array;
	Ref<StandardMaterial3D> _material;
};

class DebugRendererItem;

class DebugRenderer {
public:
	~DebugRenderer();

	void set_world(World3D *world);

	void begin();
	void draw_box(const Transform3D &t, ColorID color);
	void draw_box_mm(const Transform3D &t, Color8 color);
	void end();
	void clear();

private:
	std::vector<DebugRendererItem *> _items;
	unsigned int _current = 0;
	bool _inside_block = false;
	World3D *_world = nullptr;
	DebugMultiMeshRenderer _mm_renderer;
};

} // namespace VoxelDebug

#endif // VOXEL_DEBUG_H
