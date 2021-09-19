#include "voxel_debug.h"
#include "../util/fixed_array.h"
#include "../util/godot/direct_mesh_instance.h"
#include "../util/godot/direct_multimesh_instance.h"

#include <scene/resources/mesh.h>

namespace VoxelDebug {

FixedArray<Ref<Mesh>, ID_COUNT> g_wirecubes;
bool g_finalized = false;

template <typename T>
void raw_copy_to(PoolVector<T> &dst, const T *src, unsigned int count) {
	dst.resize(count);
	typename PoolVector<T>::Write w = dst.write();
	memcpy(w.ptr(), src, count * sizeof(T));
}

static Color get_color(ColorID id) {
	switch (id) {
		case ID_VOXEL_BOUNDS:
			return Color(1, 1, 1);
		case ID_OCTREE_BOUNDS:
			return Color(0.5, 0.5, 0.5);
		case ID_VOXEL_GRAPH_DEBUG_BOUNDS:
			return Color(1.0, 1.0, 0.0);
		case ID_WHITE:
			return Color(1, 1, 1);
		default:
			CRASH_NOW_MSG("Unexpected index");
	}
	return Color();
}

Ref<Mesh> get_wirecube(ColorID id) {
	CRASH_COND(g_finalized);

	Ref<Mesh> &wirecube = g_wirecubes[id];

	if (wirecube.is_null()) {
		const Vector3 positions_raw[] = {
			Vector3(0, 0, 0),
			Vector3(1, 0, 0),
			Vector3(1, 0, 1),
			Vector3(0, 0, 1),
			Vector3(0, 1, 0),
			Vector3(1, 1, 0),
			Vector3(1, 1, 1),
			Vector3(0, 1, 1)
		};
		PoolVector3Array positions;
		raw_copy_to(positions, positions_raw, 8);

		Color white(1.0, 1.0, 1.0);
		PoolColorArray colors;
		colors.resize(positions.size());
		{
			PoolColorArray::Write w = colors.write();
			for (int i = 0; i < colors.size(); ++i) {
				w[i] = white;
			}
		}

		const int indices_raw[] = {
			0, 1,
			1, 2,
			2, 3,
			3, 0,

			4, 5,
			5, 6,
			6, 7,
			7, 4,

			0, 4,
			1, 5,
			2, 6,
			3, 7
		};
		PoolIntArray indices;
		raw_copy_to(indices, indices_raw, 24);

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = positions;
		arrays[Mesh::ARRAY_COLOR] = colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		Ref<ArrayMesh> mesh = memnew(ArrayMesh);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

		Ref<SpatialMaterial> mat;
		mat.instance();
		mat->set_albedo(get_color(id));
		mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
		mesh->surface_set_material(0, mat);

		wirecube = mesh;
	}

	return wirecube;
}

void free_resources() {
	for (unsigned int i = 0; i < g_wirecubes.size(); ++i) {
		g_wirecubes[i].unref();
	}
	g_finalized = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class DebugRendererItem {
public:
	DebugRendererItem() {
		_mesh_instance.create();
		// TODO When shadow casting is on, directional shadows completely break.
		// The reason is still unknown.
		// It should be off anyways, but it's rather concerning.
		_mesh_instance.set_cast_shadows_setting(VisualServer::SHADOW_CASTING_SETTING_OFF);
	}

	void set_mesh(Ref<Mesh> mesh) {
		if (_mesh != mesh) {
			_mesh = mesh;
			_mesh_instance.set_mesh(mesh);
		}
	}

	void set_transform(Transform t) {
		if (_transform != t) {
			_transform = t;
			_mesh_instance.set_transform(t);
		}
	}

	void set_visible(bool visible) {
		if (_visible != visible) {
			_visible = visible;
			_mesh_instance.set_visible(visible);
		}
	}

	void set_world(World *world) {
		_mesh_instance.set_world(world);
	}

private:
	Transform _transform;
	bool _visible = true;
	Ref<Mesh> _mesh;
	DirectMeshInstance _mesh_instance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DebugRenderer::~DebugRenderer() {
	clear();
}

void DebugRenderer::clear() {
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		memdelete(*it);
	}
	_items.clear();
	_mm_renderer.clear();
}

void DebugRenderer::set_world(World *world) {
	_world = world;
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		(*it)->set_world(world);
	}
	_mm_renderer.set_world(world);
}

void DebugRenderer::begin() {
	CRASH_COND(_inside_block);
	CRASH_COND(_world == nullptr);
	_current = 0;
	_inside_block = true;
	_mm_renderer.begin();
}

void DebugRenderer::draw_box(const Transform &t, ColorID color) {
	// Pick an existing item, or create one
	DebugRendererItem *item;
	if (_current >= _items.size()) {
		item = memnew(DebugRendererItem);
		item->set_world(_world);
		_items.push_back(item);
	} else {
		item = _items[_current];
	}

	item->set_mesh(get_wirecube(color));
	item->set_transform(t);
	item->set_visible(true);

	++_current;
}

void DebugRenderer::draw_box_mm(const Transform &t, Color8 color) {
	_mm_renderer.draw_box(t, color);
}

void DebugRenderer::end() {
	CRASH_COND(!_inside_block);
	// Hide exceeding items
	for (unsigned int i = _current; i < _items.size(); ++i) {
		DebugRendererItem *item = _items[i];
		item->set_visible(false);
	}
	_inside_block = false;
	_mm_renderer.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DebugMultiMeshRenderer::DebugMultiMeshRenderer() {
	_multimesh_instance.create();
	// TODO When shadow casting is on, directional shadows completely break.
	// The reason is still unknown.
	// It should be off anyways, but it's rather concerning.
	_multimesh_instance.set_cast_shadows_setting(VisualServer::SHADOW_CASTING_SETTING_OFF);
	_multimesh.instance();
	Ref<Mesh> wirecube = get_wirecube(ID_WHITE);
	_multimesh->set_mesh(wirecube);
	_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	_multimesh->set_color_format(MultiMesh::COLOR_8BIT);
	_multimesh->set_custom_data_format(MultiMesh::CUSTOM_DATA_NONE);
	_multimesh_instance.set_multimesh(_multimesh);
	_material.instance();
	_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	_material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	_multimesh_instance.set_material_override(_material);
}

void DebugMultiMeshRenderer::set_world(World *world) {
	_multimesh_instance.set_world(world);
	_world = world;
}

void DebugMultiMeshRenderer::begin() {
	ERR_FAIL_COND(_inside_block);
	ERR_FAIL_COND(_world == nullptr);
	_inside_block = true;
}

void DebugMultiMeshRenderer::draw_box(const Transform &t, Color8 color) {
	_items.push_back(DirectMultiMeshInstance::TransformAndColor8{ t, color });
}

void DebugMultiMeshRenderer::end() {
	ERR_FAIL_COND(!_inside_block);
	_inside_block = false;

	DirectMultiMeshInstance::make_transform_and_color8_3d_bulk_array(to_span_const(_items), _bulk_array);
	if (_items.size() != static_cast<unsigned int>(_multimesh->get_instance_count())) {
		_multimesh->set_instance_count(_items.size());
	}

	// Apparently Godot doesn't like empty bulk arrays, it breaks RasterizerStorageGLES3
	if (_items.size() > 0) {
		_multimesh->set_as_bulk_array(_bulk_array);
	}

	_items.clear();
}

void DebugMultiMeshRenderer::clear() {
	_items.clear();
	_multimesh->set_instance_count(0);
}

} // namespace VoxelDebug
