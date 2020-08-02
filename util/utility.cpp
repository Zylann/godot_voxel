#include "utility.h"
#include "../cube_tables.h"

#include <core/engine.h>
#include <scene/resources/mesh.h>

bool is_surface_triangulated(Array surface) {
	PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PoolIntArray indices = surface[Mesh::ARRAY_INDEX];
	return positions.size() >= 3 && indices.size() >= 3;
}

bool is_mesh_empty(Ref<Mesh> mesh_ref) {
	if (mesh_ref.is_null()) {
		return true;
	}
	const Mesh &mesh = **mesh_ref;
	if (mesh.get_surface_count() == 0) {
		return true;
	}
	if (mesh.surface_get_array_len(0) == 0) {
		return true;
	}
	return false;
}

bool try_call_script(const Object *obj, StringName method_name, const Variant **args, unsigned int argc, Variant *out_ret) {
	ScriptInstance *script = obj->get_script_instance();
	if (script == nullptr || !script->has_method(method_name)) {
		return false;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !script->get_script()->is_tool()) {
		// Can't call a method on a non-tool script in the editor
		return false;
	}
#endif

	Variant::CallError err;
	Variant ret = script->call(method_name, args, argc, err);

	// TODO Why does Variant::get_call_error_text want a non-const Object pointer??? It only uses const methods
	ERR_FAIL_COND_V_MSG(err.error != Variant::CallError::CALL_OK, false,
			Variant::get_call_error_text(const_cast<Object *>(obj), method_name, nullptr, 0, err));
	// This had to be explicitely logged due to the usual GD debugger not working with threads

	if (out_ret != nullptr) {
		*out_ret = ret;
	}

	return true;
}

#if TOOLS_ENABLED

namespace VoxelDebug {

Ref<Mesh> g_debug_box_mesh;

void create_debug_box_mesh() {
	PoolVector3Array positions;
	positions.resize(8);
	{
		PoolVector3Array::Write w = positions.write();
		for (int i = 0; i < positions.size(); ++i) {
			w[i] = Cube::g_corner_position[i];
		}
	}
	PoolIntArray indices;
	indices.resize(Cube::EDGE_COUNT * 2);
	{
		PoolIntArray::Write w = indices.write();
		int j = 0;
		for (int i = 0; i < Cube::EDGE_COUNT; ++i) {
			w[j++] = Cube::g_edge_corners[i][0];
			w[j++] = Cube::g_edge_corners[i][1];
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	Ref<SpatialMaterial> mat;
	mat.instance();
	mat->set_albedo(Color(0, 1, 0));
	mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	mesh->surface_set_material(0, mat);
	g_debug_box_mesh = mesh;
}

void free_debug_box_mesh() {
	g_debug_box_mesh.unref();
}

Ref<Mesh> get_debug_box_mesh() {
	return g_debug_box_mesh;
}

} // namespace VoxelDebug

#endif
