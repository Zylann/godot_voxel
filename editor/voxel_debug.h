#ifndef VOXEL_DEBUG_H
#define VOXEL_DEBUG_H

#include "../util/godot/classes/standard_material_3d.h"
#include "../util/godot/direct_multimesh_instance.h"
#include <vector>

ZN_GODOT_FORWARD_DECLARE(class Mesh);

namespace zylann {
namespace DebugColors {

enum ColorID { //
	ID_VOXEL_BOUNDS = 0,
	ID_OCTREE_BOUNDS,
	ID_VOXEL_GRAPH_DEBUG_BOUNDS,
	ID_WHITE,
	ID_COUNT
};

} // namespace DebugColors

Ref<Mesh> get_debug_wirecube(DebugColors::ColorID id);
void free_debug_resources();

class DebugMultiMeshRenderer {
public:
	DebugMultiMeshRenderer();
	~DebugMultiMeshRenderer();

	void set_world(World3D *world);
	void begin();
	void draw_box(const Transform3D &t, Color8 color);
	void end();
	void clear();

private:
	std::vector<DirectMultiMeshInstance::TransformAndColor32> _items;
	Ref<MultiMesh> _multimesh;
	DirectMultiMeshInstance _multimesh_instance;
	// TODO World3D is a reference, do not store it by pointer
	World3D *_world = nullptr;
	bool _inside_block = false;
	PackedFloat32Array _bulk_array;
	Ref<StandardMaterial3D> _material;
};

class DebugRendererItem;

class DebugRenderer {
public:
	~DebugRenderer();

	// This class does not uses nodes. Call this first to choose in which world it renders.
	void set_world(World3D *world);

	// Call this before issuing drawing commands
	void begin();

	void draw_box(const Transform3D &t, DebugColors::ColorID color);

	// Draws a box wireframe using MultiMesh, allowing to draw much more without slowing down.
	// The box's origin is its lower corner. Size is defined by the transform's basis.
	void draw_box_mm(const Transform3D &t, Color8 color);

	// Call this after issuing all drawing commands
	void end();

	void clear();

private:
	std::vector<DebugRendererItem *> _items;
	unsigned int _current = 0;
	bool _inside_block = false;
	// TODO World3D is a reference, do not store it by pointer
	World3D *_world = nullptr;
	DebugMultiMeshRenderer _mm_renderer;
};

} // namespace zylann

#endif // VOXEL_DEBUG_H
