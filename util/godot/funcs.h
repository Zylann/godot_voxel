#ifndef VOXEL_UTILITY_GODOT_FUNCS_H
#define VOXEL_UTILITY_GODOT_FUNCS_H

#include <core/reference.h>
#include <core/variant.h>
#include <memory>

class Mesh;
class ConcavePolygonShape;
class MultiMesh;

bool is_surface_triangulated(Array surface);
bool is_mesh_empty(Ref<Mesh> mesh_ref);

bool try_call_script(
		const Object *obj, StringName method_name, const Variant **args, unsigned int argc, Variant *out_ret);

inline bool try_call_script(
		const Object *obj, StringName method_name, Variant arg0, Variant arg1, Variant arg2, Variant *out_ret) {
	const Variant *args[3] = { &arg0, &arg1, &arg2 };
	return try_call_script(obj, method_name, args, 3, out_ret);
}

Ref<ConcavePolygonShape> create_concave_polygon_shape(Vector<Array> surfaces);

// This API can be confusing so I made a wrapper
int get_visible_instance_count(const MultiMesh &mm);

// Generates a wireframe-mesh that highlights edges of a triangle-mesh where vertices are not shared
Array generate_debug_seams_wireframe_surface(Ref<Mesh> src_mesh, int surface_index);

// `(ref1 = ref2).is_valid()` does not work because Ref<T> does not implement an `operator=` returning the value.
// So instead we can write it as `try_get_as(ref2, ref1)`
template <typename From_T, typename To_T>
inline bool try_get_as(Ref<From_T> from, Ref<To_T> &to) {
	to = from;
	return to.is_valid();
}

// Creates a shared_ptr which will use Godot's allocation functions
template <typename T>
inline std::shared_ptr<T> gd_make_shared() {
	// std::make_shared() apparently wont allow us to specify custom new and delete
	return std::shared_ptr<T>(memnew(T), memdelete<T>);
}

template <typename T, typename Arg_T>
inline std::shared_ptr<T> gd_make_shared(Arg_T arg) {
	return std::shared_ptr<T>(memnew(T(arg)), memdelete<T>);
}

template <typename T, typename Arg0_T, typename Arg1_T>
inline std::shared_ptr<T> gd_make_shared(Arg0_T arg0, Arg1_T arg1) {
	return std::shared_ptr<T>(memnew(T(arg0, arg1)), memdelete<T>);
}

#endif // VOXEL_UTILITY_GODOT_FUNCS_H
