#include "debug_renderer.h"
#include "../containers/fixed_array.h"
#include "../memory/memory.h"
#include "classes/array_mesh.h"
#include "core/packed_arrays.h"
#include "direct_mesh_instance.h"
#include "direct_multimesh_instance.h"

namespace zylann::godot {

namespace {

Ref<Mesh> create_debug_wirecube(Color color) {
	const Vector3 positions_raw[] = {
		Vector3(0, 0, 0), //
		Vector3(1, 0, 0), //
		Vector3(1, 0, 1), //
		Vector3(0, 0, 1), //
		Vector3(0, 1, 0), //
		Vector3(1, 1, 0), //
		Vector3(1, 1, 1), //
		Vector3(0, 1, 1) //
	};
	PackedVector3Array positions;
	copy_to(positions, Span<const Vector3>(positions_raw, 8));

	PackedColorArray colors;
	// Not pre-resizing because writing to arrays have different syntax between Godot modules and GodotCpp.
	for (int i = 0; i < positions.size(); ++i) {
		colors.push_back(color);
	}

	const int32_t indices_raw[] = {
		0, 1, //
		1, 2, //
		2, 3, //
		3, 0, //

		4, 5, //
		5, 6, //
		6, 7, //
		7, 4, //

		0, 4, //
		1, 5, //
		2, 6, //
		3, 7 //
	};
	PackedInt32Array indices;
	copy_to(indices, Span<const int32_t>(indices_raw, 24));

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

	return mesh;
}

} // namespace

DebugRenderer::DebugRenderer() {}

void DebugRenderer::init() {
	_multimesh_instance.create();
	// TODO When shadow casting is on, directional shadows completely break.
	// The reason is still unknown.
	// It should be off anyways, but it's rather concerning.
	_multimesh_instance.set_cast_shadows_setting(RenderingServer::SHADOW_CASTING_SETTING_OFF);
	_multimesh.instantiate();
	Ref<Mesh> wirecube = create_debug_wirecube(Color(1, 1, 1));
	_multimesh->set_mesh(wirecube);
	_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	// TODO Optimize: Godot needs to bring back 8-bit color attributes on multimesh, 32-bit colors are too much
	//_multimesh->set_color_format(MultiMesh::COLOR_8BIT);
	_multimesh->set_use_colors(true);
	_multimesh->set_use_custom_data(false);
	_multimesh_instance.set_multimesh(_multimesh);
	_material.instantiate();
	_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	_multimesh_instance.set_material_override(_material);
}

DebugRenderer::~DebugRenderer() {
	// A MultiMeshInstance created without nodes does not hold ownership on its material.
	// So we need to destroy it before we release ownership at the end of this destructor.
	// Otherwise RenderingServer produces errors.
	_multimesh_instance.destroy();
}

void DebugRenderer::set_world(World3D *world) {
	if (_initialized == false) {
		init();
		_initialized = true;
	}

	_multimesh_instance.set_world(world);
	_world = world;
}

void DebugRenderer::begin() {
	ERR_FAIL_COND(_inside_block);
	ERR_FAIL_COND(_world == nullptr);
	_inside_block = true;
}

void DebugRenderer::draw_box(const Transform3D &t, Color8 color) {
	_items.push_back(DirectMultiMeshInstance::TransformAndColor32{ t, Color(color) });
}

void DebugRenderer::end() {
	ERR_FAIL_COND(!_inside_block);
	_inside_block = false;

	DirectMultiMeshInstance::make_transform_and_color32_3d_bulk_array(to_span_const(_items), _bulk_array);
	if (_items.size() != static_cast<unsigned int>(_multimesh->get_instance_count())) {
		_multimesh->set_instance_count(_items.size());
	}

	// Apparently Godot doesn't like empty bulk arrays, it breaks RasterizerStorageGLES3
	if (_items.size() > 0) {
		//_multimesh->set_as_bulk_array(_bulk_array);
		RenderingServer::get_singleton()->multimesh_set_buffer(_multimesh->get_rid(), _bulk_array);
	}

	_items.clear();
}

void DebugRenderer::clear() {
	_items.clear();
	// Can be null if `init()` hasn't been called
	if (_multimesh.is_valid()) {
		_multimesh->set_instance_count(0);
	}
}

} // namespace zylann::godot
