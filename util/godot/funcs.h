#ifndef VOXEL_UTILITY_GODOT_FUNCS_H
#define VOXEL_UTILITY_GODOT_FUNCS_H

#include "../math/vector2f.h"
#include "../math/vector3f.h"
#include "../span.h"

#include <core/object/ref_counted.h>
#include <core/variant/variant.h>

#include <memory>

class Mesh;
class ConcavePolygonShape3D;
class MultiMesh;
class Node;

namespace zylann {

bool is_surface_triangulated(Array surface);
bool is_mesh_empty(Ref<Mesh> mesh_ref);

bool try_call_script(
		const Object *obj, StringName method_name, const Variant **args, unsigned int argc, Variant *out_ret);

inline bool try_call_script(
		const Object *obj, StringName method_name, Variant arg0, Variant arg1, Variant arg2, Variant *out_ret) {
	const Variant *args[3] = { &arg0, &arg1, &arg2 };
	return try_call_script(obj, method_name, args, 3, out_ret);
}

Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces);

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

template <typename T, typename Arg0_T, typename Arg1_T, typename Arg2_T>
inline std::shared_ptr<T> gd_make_shared(Arg0_T arg0, Arg1_T arg1, Arg2_T arg2) {
	return std::shared_ptr<T>(memnew(T(arg0, arg1, arg2)), memdelete<T>);
}

// For use with smart pointers such as std::unique_ptr
template <typename T>
struct GodotObjectDeleter {
	inline void operator()(T *obj) {
		memdelete(obj);
	}
};

// Specialization of `std::unique_ptr which uses `memdelete()` as deleter.
template <typename T>
using GodotUniqueObjectPtr = std::unique_ptr<T, GodotObjectDeleter<T>>;

// Creates a `GodotUniqueObjectPtr<T>` with an object constructed with `memnew()` inside.
template <typename T>
GodotUniqueObjectPtr<T> gd_make_unique() {
	return GodotUniqueObjectPtr<T>(memnew(T));
}

void set_nodes_owner(Node *root, Node *owner);
void set_nodes_owner_except_root(Node *root, Node *owner);

// To allow using Ref<T> as key in Godot's HashMap
template <typename T>
struct RefHasher {
	static _FORCE_INLINE_ uint32_t hash(const Ref<T> &v) {
		return uint32_t(uint64_t(v.ptr())) * (0x9e3779b1L);
	}
};

void copy_to(Vector<Vector3> &dst, const std::vector<Vector3f> &src);
void copy_to(Vector<Vector2> &dst, const std::vector<Vector2f> &src);

inline Vector2f to_vec2f(Vector2i v) {
	return Vector2f(v.x, v.y);
}

inline Vector2f to_vec2f(Vector2 v) {
	return Vector2f(v.x, v.y);
}

inline Vector3f to_vec3f(Vector3i v) {
	return Vector3f(v.x, v.y, v.z);
}

inline Vector3f to_vec3f(Vector3 v) {
	return Vector3f(v.x, v.y, v.z);
}

inline String to_godot(const std::string_view sv) {
	return String::utf8(sv.data(), sv.size());
}

static PackedStringArray to_godot(const std::vector<std::string_view> &svv) {
	PackedStringArray psa;
	psa.resize(svv.size());
	for (unsigned int i = 0; i < svv.size(); ++i) {
		psa.write[i] = to_godot(svv[i]);
	}
	return psa;
}

static PackedStringArray to_godot(const std::vector<std::string> &sv) {
	PackedStringArray psa;
	psa.resize(sv.size());
	for (unsigned int i = 0; i < sv.size(); ++i) {
		psa.write[i] = to_godot(sv[i]);
	}
	return psa;
}

template <typename T>
Span<const T> to_span_const(const Vector<T> &a) {
	return Span<const T>(a.ptr(), 0, a.size());
}

} // namespace zylann

#endif // VOXEL_UTILITY_GODOT_FUNCS_H
