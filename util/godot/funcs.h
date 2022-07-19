#ifndef VOXEL_UTILITY_GODOT_FUNCS_H
#define VOXEL_UTILITY_GODOT_FUNCS_H

#include "../math/vector2f.h"
#include "../math/vector3f.h"
#include "../span.h"

#include <core/object/ref_counted.h>

#include <functional>
#include <iosfwd>
#include <memory>

class Mesh;
class ConcavePolygonShape3D;
class MultiMesh;
class Node;

namespace zylann {

bool is_surface_triangulated(Array surface);
bool is_mesh_empty(const Mesh &mesh);

// Combines all mesh surface arrays into one collider.
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces);
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Vector3f> positions, Span<const int> indices);
// Create shape from a sub-region of a mesh surface (starting at 0).
Ref<ConcavePolygonShape3D> create_concave_polygon_shape(const Array surface_arrays, unsigned int index_count);

// This API can be confusing so I made a wrapper
int get_visible_instance_count(const MultiMesh &mm);

// Generates a wireframe-mesh that highlights edges of a triangle-mesh where vertices are not shared
Array generate_debug_seams_wireframe_surface(const Mesh &src_mesh, int surface_index);

// `(ref1 = ref2).is_valid()` does not work because Ref<T> does not implement an `operator=` returning the value.
// So instead we can write it as `try_get_as(ref2, ref1)`
template <typename From_T, typename To_T>
inline bool try_get_as(const Ref<From_T> &from, Ref<To_T> &to) {
	to = from;
	return to.is_valid();
}

// Creates a shared_ptr which will always use Godot's allocation functions
template <typename T>
inline std::shared_ptr<T> gd_make_shared() {
	// std::make_shared() apparently wont allow us to specify custom new and delete
	return std::shared_ptr<T>(memnew(T), memdelete<T>);
}

// For use with smart pointers such as std::unique_ptr
template <typename T>
struct GodotObjectDeleter {
	inline void operator()(T *obj) {
		memdelete(obj);
	}
};

// Specialization of `std::unique_ptr which always uses Godot's `memdelete()` as deleter.
template <typename T>
using GodotObjectUniquePtr = std::unique_ptr<T, GodotObjectDeleter<T>>;

// Creates a `GodotObjectUniquePtr<T>` with an object constructed with `memnew()` inside.
template <typename T>
GodotObjectUniquePtr<T> gd_make_unique() {
	return GodotObjectUniquePtr<T>(memnew(T));
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

template <typename T>
void raw_copy_to(Vector<T> &to, const std::vector<T> &from) {
	to.resize(from.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(from.size() != static_cast<size_t>(to.size()));
	memcpy(to.ptrw(), from.data(), from.size() * sizeof(T));
}

inline String to_godot(const std::string_view sv) {
	return String::utf8(sv.data(), sv.size());
}

inline Vector2f to_vec2f(Vector2 v) {
	return Vector2f(v.x, v.y);
}

inline Vector2f to_vec2f(Vector2i v) {
	return Vector2f(v.x, v.y);
}

inline Vector3 get_forward(const Transform3D &t) {
	return -t.basis.get_column(Vector3::AXIS_Z);
}

inline Vector3 to_godot(const Vector3f v) {
	return Vector3(v.x, v.y, v.z);
}

// Turns out these functions are only used in editor for now.
// They are generic, but I have to wrap them, otherwise GCC throws warnings-as-errors for them being unused.
#ifdef TOOLS_ENABLED

PackedStringArray to_godot(const std::vector<std::string_view> &svv);
PackedStringArray to_godot(const std::vector<std::string> &sv);

#endif

// template <typename T>
// Span<const T> to_span_const(const Vector<T> &a) {
// 	return Span<const T>(a.ptr(), 0, a.size());
// }

inline Span<const Vector2> to_span(const PackedVector2Array &a) {
	return Span<const Vector2>(a.ptr(), a.size());
}

inline Span<const Vector3> to_span(const PackedVector3Array &a) {
	return Span<const Vector3>(a.ptr(), a.size());
}

inline Span<const int> to_span(const PackedInt32Array &a) {
	return Span<const int>(a.ptr(), a.size());
}

inline String ptr2s(const void *p) {
	return String::num_uint64((uint64_t)p, 16);
}

} // namespace zylann

// Needed for `zylann::format()`.
// I gave up trying to nicely convert Godot's String here... it has non-explicit `const char*` constructor, that makes
// other overloads ambiguous...
//std::stringstream &operator<<(std::stringstream &ss, const String &s);
struct GodotStringWrapper {
	GodotStringWrapper(const String &p_s) : s(p_s) {}
	const String &s;
};
std::stringstream &operator<<(std::stringstream &ss, GodotStringWrapper s);

namespace std {

// For String keys in std::unordered_map
template <>
struct hash<String> {
	inline size_t operator()(const String &v) const {
		return v.hash();
	}
};

// For Ref<T> keys in std::unordered_map, hashed by pointer, not by content
template <typename T>
struct hash<Ref<T>> {
	inline size_t operator()(const Ref<T> &v) const {
		return std::hash<const T *>{}(v.ptr());
	}
};

} // namespace std

#endif // VOXEL_UTILITY_GODOT_FUNCS_H
