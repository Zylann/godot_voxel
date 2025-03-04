#include "voxel_blocky_library_base.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/containers/small_vector.h"
#include "../../util/math/box2f.h"
#include "../../util/math/triangle.h"
#include "../../util/math/vector4f.h"
#include "../../util/profiling.h"
// #include "../../util/string/format.h" // DEBUG
#include "voxel_blocky_model.h"
#include "voxel_mesher_blocky.h"
#include <bitset>

namespace zylann::voxel {

void VoxelBlockyLibraryBase::load_default() {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
}

void VoxelBlockyLibraryBase::clear() {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
}

void VoxelBlockyLibraryBase::bake() {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
}

void VoxelBlockyLibraryBase::set_bake_tangents(bool bt) {
	_bake_tangents = bt;
	_needs_baking = true;
}

TypedArray<Material> VoxelBlockyLibraryBase::_b_get_materials() const {
	// Note, if at least one non-empty voxel has no material, there will be one null entry in this list to represent
	// "The default material".
	TypedArray<Material> materials;
	materials.resize(_indexed_materials.size());
	for (size_t i = 0; i < _indexed_materials.size(); ++i) {
		materials[i] = _indexed_materials[i];
	}
	return materials;
}

void VoxelBlockyLibraryBase::_b_bake() {
	bake();
}

Ref<Material> VoxelBlockyLibraryBase::get_material_by_index(unsigned int index) const {
	ZN_ASSERT_RETURN_V(index < _indexed_materials.size(), Ref<Material>());
	return _indexed_materials[index];
}

unsigned int VoxelBlockyLibraryBase::get_material_index_count() const {
	return _indexed_materials.size();
}

#ifdef TOOLS_ENABLED

void VoxelBlockyLibraryBase::get_configuration_warnings(PackedStringArray &out_warnings) const {
	// Implemented in child classes
}

#endif

void VoxelBlockyLibraryBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_materials"), &VoxelBlockyLibraryBase::_b_get_materials);

	ClassDB::bind_method(D_METHOD("get_bake_tangents"), &VoxelBlockyLibraryBase::get_bake_tangents);
	ClassDB::bind_method(D_METHOD("set_bake_tangents", "bake_tangents"), &VoxelBlockyLibraryBase::set_bake_tangents);

	ClassDB::bind_method(D_METHOD("bake"), &VoxelBlockyLibraryBase::_b_bake);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_tangents"), "set_bake_tangents", "get_bake_tangents");

	BIND_CONSTANT(MAX_MODELS);
	BIND_CONSTANT(MAX_MATERIALS);
}

namespace {

struct QuadIndices {
	// 3---2
	// |   |
	// 0---1
	unsigned int i0;
	unsigned int i1;
	unsigned int i2;
	unsigned int i3;

	Box2f to_box(Span<const Vector2f> vertices) const {
		const Vector2f p0 = vertices[i0];
		const Vector2f p2 = vertices[i2];
		return Box2f(p0, p2 - p0);
	}
};

bool detect_single_quad(Span<const Vector2f> vertices, Span<const int32_t> indices, QuadIndices &out_quad_indices) {
	if (vertices.size() != 4) {
		return false;
	}
	if (indices.size() != 6) {
		return false;
	}
	Vector2f minp = vertices[0];
	Vector2f maxp = minp;
	for (Vector2f v : vertices) {
		minp = math::min(v, minp);
		maxp = math::max(v, maxp);
	}
	const Vector2f p0(minp);
	const Vector2f p1(maxp.x, minp.y);
	const Vector2f p2(maxp);
	const Vector2f p3(minp.x, maxp.y);

	int i0 = -1;
	int i1 = -1;
	int i2 = -1;
	int i3 = -1;

	for (unsigned int i = 0; i < vertices.size(); ++i) {
		const Vector2f v = vertices[i];
		if (math::is_equal_approx(v, p0)) {
			i0 = i;
		} else if (math::is_equal_approx(v, p1)) {
			i1 = i;
		} else if (math::is_equal_approx(v, p2)) {
			i2 = i;
		} else if (math::is_equal_approx(v, p3)) {
			i3 = i;
		} else {
			return false;
		}
	}

	if (i0 == -1 || i1 == -1 || i2 == -1 || i3 == -1) {
		return false;
	}

	out_quad_indices = QuadIndices{ static_cast<unsigned int>(i0),
									static_cast<unsigned int>(i1),
									static_cast<unsigned int>(i2),
									static_cast<unsigned int>(i3) };
	return true;
}

void flip_winding(Span<int32_t> indices) {
	ZN_ASSERT((indices.size() % 3) == 0);
	for (unsigned int i = 0; i < indices.size(); i += 3) {
		std::swap(indices[i + 1], indices[i + 2]);
	}
}

void to_2d(Span<const Vector3f> src, Span<Vector2f> dst, unsigned int src_x_axis, unsigned int src_y_axis) {
	ZN_ASSERT(src.size() == dst.size());
	for (unsigned int i = 0; i < src.size(); ++i) {
		const Vector3f srcv = src[i];
		const Vector2f dstv(srcv[src_x_axis], srcv[src_y_axis]);
		dst[i] = dstv;
	}
}

void to_2d(Span<const Vector3f> src, Span<Vector2f> dst, unsigned int side) {
	// TODO Flip winding?
	switch (side) {
		case Cube::SIDE_NEGATIVE_X:
			to_2d(src, dst, math::AXIS_Z, math::AXIS_Y);
			break;
		case Cube::SIDE_POSITIVE_X:
			to_2d(src, dst, math::AXIS_Z, math::AXIS_Y);
			break;
		case Cube::SIDE_NEGATIVE_Y:
			to_2d(src, dst, math::AXIS_X, math::AXIS_Z);
			break;
		case Cube::SIDE_POSITIVE_Y:
			to_2d(src, dst, math::AXIS_X, math::AXIS_Z);
			break;
		case Cube::SIDE_NEGATIVE_Z:
			to_2d(src, dst, math::AXIS_X, math::AXIS_Y);
			break;
		case Cube::SIDE_POSITIVE_Z:
			to_2d(src, dst, math::AXIS_X, math::AXIS_Y);
			break;
	}
}

void to_3d(
		Span<const Vector2f> src,
		Span<Vector3f> dst,
		unsigned int dst_x_axis,
		unsigned int dst_y_axis,
		unsigned int dst_z_axis,
		float z
) {
	ZN_ASSERT(src.size() == dst.size());
	for (unsigned int i = 0; i < src.size(); ++i) {
		const Vector2f srcv = src[i];
		Vector3f dstv;
		dstv[dst_x_axis] = srcv.x;
		dstv[dst_y_axis] = srcv.y;
		dstv[dst_z_axis] = z;
		dst[i] = dstv;
	}
}

void to_3d(Span<const Vector2f> src, Span<Vector3f> dst, unsigned int side) {
	switch (side) {
		case Cube::SIDE_NEGATIVE_X:
			to_3d(src, dst, math::AXIS_Z, math::AXIS_Y, math::AXIS_X, 0.f);
			break;
		case Cube::SIDE_POSITIVE_X:
			to_3d(src, dst, math::AXIS_Z, math::AXIS_Y, math::AXIS_X, 1.f);
			break;
		case Cube::SIDE_NEGATIVE_Y:
			to_3d(src, dst, math::AXIS_X, math::AXIS_Z, math::AXIS_Y, 0.f);
			break;
		case Cube::SIDE_POSITIVE_Y:
			to_3d(src, dst, math::AXIS_X, math::AXIS_Z, math::AXIS_Y, 1.f);
			break;
		case Cube::SIDE_NEGATIVE_Z:
			to_3d(src, dst, math::AXIS_X, math::AXIS_Y, math::AXIS_Z, 0.f);
			break;
		case Cube::SIDE_POSITIVE_Z:
			to_3d(src, dst, math::AXIS_X, math::AXIS_Y, math::AXIS_Z, 1.f);
			break;
	}
}

} // namespace

namespace blocky {

void get_side_geometry_2d_all_surfaces(
		const BakedModel::Model &model,
		int side,
		StdVector<Vector2f> &out_vertices,
		StdVector<int32_t> &out_indices
) {
	unsigned int vertex_count = 0;
	unsigned int index_count = 0;

	const FixedArray<BakedModel::SideSurface, MAX_SURFACES> &side_surfaces = model.sides_surfaces[side];

	for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
		const BakedModel::SideSurface &side_surface = side_surfaces[surface_index];
		vertex_count += side_surface.positions.size();
		index_count += side_surface.indices.size();
	}

	out_vertices.resize(vertex_count);
	out_indices.resize(index_count);

	unsigned int vertex_start = 0;
	unsigned int index_start = 0;

	for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
		const BakedModel::SideSurface &side_surface = side_surfaces[surface_index];

		Span<const Vector3f> src_vertices = to_span(side_surface.positions);
		Span<const int32_t> src_indices = to_span(side_surface.indices);

		Span<Vector2f> dst_vertices = to_span(out_vertices).sub(vertex_start, side_surface.positions.size());
		Span<int32_t> dst_indices = to_span(out_indices).sub(index_start, side_surface.indices.size());

		to_2d(src_vertices, dst_vertices, side);

		for (unsigned int i = 0; i < src_indices.size(); ++i) {
			dst_indices[i] = src_indices[i] + vertex_start;
		}

		vertex_start += side_surface.positions.size();
		index_start += side_surface.indices.size();
	}
}

} // namespace blocky

namespace {

void quads_to_triangles(Span<const Box2f> quads, StdVector<Vector2f> &vertices) {
	vertices.reserve(quads.size() * 6);
	for (const Box2f &quad : quads) {
		// 3---2
		// |   |
		// 0---1
		const Vector2f &p0 = quad.position;
		const Vector2f p2 = quad.position + quad.size;
		const Vector2f p1(p2.x, p0.y);
		const Vector2f p3(p0.x, p2.y);

		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p0);
		vertices.push_back(p2);
		vertices.push_back(p3);
	}
}

int find_approx(Span<const Vector2f> vertices, const Vector2f p_v) {
	for (unsigned int i = 0; i < vertices.size(); ++i) {
		if (math::is_equal_approx(vertices[i], p_v)) {
			return i;
		}
	}
	return -1;
}

void index_triangles(
		Span<const Vector2f> src_vertices,
		StdVector<Vector2f> &dst_vertices,
		StdVector<int32_t> &dst_indices
) {
	for (const Vector2f srcv : src_vertices) {
		int dsti = find_approx(to_span(dst_vertices), srcv);
		if (dsti == -1) {
			dsti = dst_vertices.size();
			dst_vertices.push_back(srcv);
		}
		dst_indices.push_back(dsti);
	}
}

void grow_triangle(Vector2f &a, Vector2f &b, Vector2f &c, float by) {
	const Vector2f m = 0.333333f * (a + b + c);
	a += by * (a - m);
	b += by * (b - m);
	c += by * (c - m);
}

bool find_triangle(Span<const Vector2f> vertices, Span<const int32_t> indices, Vector2f pos, Vector3i &out_indices) {
	ZN_ASSERT((indices.size() % 3) == 0);

	for (unsigned int ii = 0; ii < indices.size(); ii += 3) {
		const int32_t i0 = indices[ii];
		const int32_t i1 = indices[ii + 1];
		const int32_t i2 = indices[ii + 2];

		Vector2f p0 = vertices[i0];
		Vector2f p1 = vertices[i1];
		Vector2f p2 = vertices[i2];

		// Grow triangle a little to workaround float precision hell...
		grow_triangle(p0, p1, p2, 0.0001f);

		if (math::is_point_in_triangle(pos, p0, p1, p2)) {
			out_indices.x = i0;
			out_indices.y = i1;
			out_indices.z = i2;
			return true;
		}
		//  else {
		// 	ZN_PRINT_VERBOSE(format("Not in {}, {}, {}", p0, p1, p2));
		// }
	}

	return false;
}

void interpolate_attributes_assume_no_seams(
		Span<const Vector2f> src_vertices,
		Span<const int32_t> src_indices,
		Span<const Vector2f> src_uvs,
		Span<const float> src_tangents,
		Span<const Vector2f> interp_vertices,
		Span<Vector2f> interp_uvs,
		Span<float> interp_tangents
) {
	ZN_ASSERT((src_indices.size() % 3) == 0);

	// This method only works if the whole mesh we are interpolating has consistent UVs. The mesh could have seams
	// with different UVs, and vertices we are interpolating could be located exactly on those seams, leading to
	// ambiguous results. We would need a proper function or library to do proper 2D mesh subtraction preserving
	// attributes, so that we would not need this in the first place.

	const bool has_tangents = src_tangents.size() > 0;
	Span<const Vector4f> src_tangents2;
	Span<Vector4f> interp_tangents2;
	if (has_tangents) {
		// Alias tangents to be nicer to work with
		src_tangents2 = src_tangents.reinterpret_cast_to<const Vector4f>();
		interp_tangents2 = interp_tangents.reinterpret_cast_to<Vector4f>();
	}

	for (unsigned int i = 0; i < interp_vertices.size(); ++i) {
		const Vector2f interp_pos = interp_vertices[i];

		Vector3i triangle_indices;
		if (!find_triangle(src_vertices, src_indices, interp_pos, triangle_indices)) {
			// TODO Might happen on edges due to floats being annoying?
			ZN_PRINT_ERROR("Triangle not found");
			interp_uvs[i] = Vector2f();
			continue;
		}

		const Vector2f p0 = src_vertices[triangle_indices.x];
		const Vector2f p1 = src_vertices[triangle_indices.y];
		const Vector2f p2 = src_vertices[triangle_indices.z];

		const Vector3f weights = math::get_triangle_barycentric_coordinates(p0, p1, p2, interp_pos);

		const Vector2f uv0 = src_uvs[triangle_indices.x];
		const Vector2f uv1 = src_uvs[triangle_indices.y];
		const Vector2f uv2 = src_uvs[triangle_indices.z];
		interp_uvs[i] = uv0 * weights.x + uv1 * weights.y + uv2 * weights.z;

		if (has_tangents) {
			const Vector4f t0 = src_tangents2[triangle_indices.x];
			const Vector4f t1 = src_tangents2[triangle_indices.y];
			const Vector4f t2 = src_tangents2[triangle_indices.z];
			interp_tangents2[i] = t0 * weights.x + t1 * weights.y + t2 * weights.z;
		}
	}
}

} // namespace

namespace blocky {

void generate_cutout_side_surface(
		const BakedModel::SideSurface &side_surface,
		unsigned int side,
		Box2f other_quad,
		BakedModel::SideSurface &cut_side_surface
) {
	// Arguably, some of this could be done once up front.
	// Not done currently because what we really want here is a full-blown mesh boolean operation. The quad stuff is
	// only here as an early shortcut because I couldn't find how to do the former.

	// TODO Candidate for temp allocator
	StdVector<Vector2f> vertices_2d;
	vertices_2d.resize(side_surface.positions.size());
	to_2d(to_span(side_surface.positions), to_span(vertices_2d), side);

	QuadIndices quad_indices;
	const bool is_quad = detect_single_quad( //
			to_span(vertices_2d), //
			to_span(side_surface.indices), //
			quad_indices //
	);
	if (!is_quad) {
		// TODO Need a generic cutting algorith using boolean mesh operation
		// Could not find any easy way to do this
		return;
	}
	const Box2f quad = quad_indices.to_box(to_span(vertices_2d));

	SmallVector<Box2f, 6> quads;
	quad.difference_to_vec(other_quad, quads);
	// TODO Remove degenerate triangles just in case of float precision errors?

	StdVector<Vector2f> cut_vertices_2d_non_indexed;
	quads_to_triangles(to_span(quads), cut_vertices_2d_non_indexed);

	StdVector<Vector2f> cut_vertices_2d;
	StdVector<int32_t> cut_indices;
	index_triangles(to_span(cut_vertices_2d_non_indexed), cut_vertices_2d, cut_indices);

	StdVector<Vector2f> cut_uvs;
	StdVector<float> cut_tangents;
	cut_uvs.resize(cut_vertices_2d.size());
	const bool has_tangents = side_surface.tangents.size() > 0;
	if (has_tangents) {
		cut_tangents.resize(cut_vertices_2d.size() * 4);
	}
	// Recover vertex attributes by interpolating over the original geometry
	interpolate_attributes_assume_no_seams( //
			to_span(vertices_2d), //
			to_span(side_surface.indices), //
			to_span(side_surface.uvs), //
			to_span(side_surface.tangents), //
			to_span(cut_vertices_2d), //
			to_span(cut_uvs), //
			to_span(cut_tangents) //
	);

	if (side == Cube::SIDE_NEGATIVE_X || side == Cube::SIDE_NEGATIVE_Y || side == Cube::SIDE_POSITIVE_Z) {
		flip_winding(to_span(cut_indices));
	}

	StdVector<Vector3f> cut_vertices;
	cut_vertices.resize(cut_vertices_2d.size());
	to_3d(to_span(cut_vertices_2d), to_span(cut_vertices), side);

	cut_side_surface.positions = std::move(cut_vertices);
	cut_side_surface.indices = std::move(cut_indices);
	cut_side_surface.uvs = std::move(cut_uvs);
	cut_side_surface.tangents = std::move(cut_tangents);
}

void generate_model_cutout_sides(BakedModel &model_data, const uint16_t model_id, BakedLibrary &lib) {
	// Iterating other models instead of the shape matrix because there is often a limited subset of models we have
	// to compute cutouts with. Typically, this is used with transparent neighbors that cull our own faces
	for (uint16_t other_model_id = 0; other_model_id < lib.models.size(); ++other_model_id) {
		if (other_model_id == model_id) {
			continue;
		}

		BakedModel &other_model_data = lib.models[other_model_id];

		if (!other_model_data.culls_neighbors) {
			continue;
		}

		for (uint16_t side = 0; side < Cube::SIDE_COUNT; ++side) {
			// Test if the face is totally occluded first
			if (!is_face_visible(lib, model_data, other_model_id, side)) {
				continue;
			}
			if (is_face_visible_regardless_of_shape(model_data, other_model_data)) {
				continue;
			}

			// The face is partially or totally visible, depending on the neighbor's shape. Compute its cutout?

			BakedModel::Model &model = model_data.model;
			const BakedModel::Model &other_model = other_model_data.model;

			const uint16_t other_side = Cube::g_opposite_side[side];

			// TODO Candidate for temp allocator
			StdVector<Vector2f> other_all_vertices_2d;
			StdVector<int32_t> other_all_indices;
			get_side_geometry_2d_all_surfaces(other_model, other_side, other_all_vertices_2d, other_all_indices);

			QuadIndices other_quad_indices;
			const bool other_is_quad = detect_single_quad( //
					to_span(other_all_vertices_2d), //
					to_span(other_all_indices), //
					other_quad_indices //
			);
			if (!other_is_quad) {
				// Cutting not supported, we'll fallback on rendering the full side
				continue;
			}
			const Box2f other_quad = other_quad_indices.to_box(to_span(other_all_vertices_2d));

			const uint32_t other_side_shape_id = other_model.side_pattern_indices[other_side];

			FixedArray<BakedModel::SideSurface, MAX_SURFACES> cut_surfaces;

			const FixedArray<BakedModel::SideSurface, MAX_SURFACES> &side_surfaces = model.sides_surfaces[side];

			for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
				const BakedModel::SideSurface &side_surface = side_surfaces[surface_index];
				generate_cutout_side_surface(side_surface, side, other_quad, cut_surfaces[surface_index]);
			}

			// Currently, the cutout can be empty in case of failure, in which case we must fallback to full side.
			// But if we implement proper boolean operation later, that may have to change
			if (!is_empty(cut_surfaces)) {
				// Insert surfaces
				model.cutout_side_surfaces[side][other_side_shape_id] = std::move(cut_surfaces);
			}
		}
	}
}

void generate_library_cutout_sides(BakedLibrary &lib) {
	ZN_PROFILE_SCOPE();

	for (uint16_t model_id = 0; model_id < lib.models.size(); ++model_id) {
		BakedModel &model_data = lib.models[model_id];

		if (!model_data.cutout_sides_enabled) {
			continue;
		}

		generate_model_cutout_sides(model_data, model_id, lib);
	}
}

} // namespace blocky

template <typename F>
void rasterize_triangle_barycentric(Vector2f a, Vector2f b, Vector2f c, F output_func) {
	// Slower than scanline method, but looks better

	// Grow the triangle a tiny bit, to help against floating point error
	grow_triangle(a, b, c, 0.001f);

	using namespace math;

	const int min_x = (int)Math::floor(min(min(a.x, b.x), c.x));
	const int min_y = (int)Math::floor(min(min(a.y, b.y), c.y));
	const int max_x = (int)Math::ceil(max(max(a.x, b.x), c.x));
	const int max_y = (int)Math::ceil(max(max(a.y, b.y), c.y));

	// We test against points centered on grid cells
	const Vector2f offset(0.5, 0.5);

	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			if (is_point_in_triangle(Vector2f(x, y) + offset, a, b, c)) {
				output_func(x, y);
			}
		}
	}
}

namespace {
static const unsigned int RASTER_SIZE = 32;
} // namespace

void rasterize_side(
		const Span<const Vector3f> vertices,
		const Span<const int> indices,
		const unsigned int side,
		std::bitset<RASTER_SIZE * RASTER_SIZE> &bitmap
) {
	ZN_ASSERT_RETURN((indices.size() % 3) == 0);

	// For each triangle
	for (unsigned int j = 0; j < indices.size(); j += 3) {
		const Vector3f va = vertices[indices[j]];
		const Vector3f vb = vertices[indices[j + 1]];
		const Vector3f vc = vertices[indices[j + 2]];

		// Convert 3D vertices into 2D
		Vector2f a, b, c;
		switch (side) {
			case Cube::SIDE_NEGATIVE_X:
			case Cube::SIDE_POSITIVE_X:
				a = Vector2f(va.y, va.z);
				b = Vector2f(vb.y, vb.z);
				c = Vector2f(vc.y, vc.z);
				break;

			case Cube::SIDE_NEGATIVE_Y:
			case Cube::SIDE_POSITIVE_Y:
				a = Vector2f(va.x, va.z);
				b = Vector2f(vb.x, vb.z);
				c = Vector2f(vc.x, vc.z);
				break;

			case Cube::SIDE_NEGATIVE_Z:
			case Cube::SIDE_POSITIVE_Z:
				a = Vector2f(va.x, va.y);
				b = Vector2f(vb.x, vb.y);
				c = Vector2f(vc.x, vc.y);
				break;

			default:
				CRASH_NOW();
		}

		a *= RASTER_SIZE;
		b *= RASTER_SIZE;
		c *= RASTER_SIZE;

		// Rasterize pattern
		rasterize_triangle_barycentric(a, b, c, [&bitmap](unsigned int x, unsigned int y) {
			if (x >= RASTER_SIZE || y >= RASTER_SIZE) {
				return;
			}
			const unsigned int i = x + y * RASTER_SIZE;
			bitmap.set(i);
		});
	}
}

namespace blocky {

void rasterize_side_all_surfaces(
		const BakedModel &model_data,
		const unsigned int side_index,
		std::bitset<RASTER_SIZE * RASTER_SIZE> &bitmap
) {
	const FixedArray<BakedModel::SideSurface, MAX_SURFACES> &side_surfaces =
			model_data.model.sides_surfaces[side_index];
	// For each surface (they are all combined for simplicity, though it is also a limitation)
	for (unsigned int surface_index = 0; surface_index < model_data.model.surface_count; ++surface_index) {
		const BakedModel::SideSurface &side = side_surfaces[surface_index];
		rasterize_side(to_span(side.positions), to_span(side.indices), side_index, bitmap);
	}
}

void generate_side_culling_matrix(BakedLibrary &baked_data) {
	ZN_PROFILE_SCOPE();
	// When two blocky voxels are next to each other, they share a side.
	// Geometry of either side can be culled away if covered by the other,
	// but it's very expensive to do a full polygon check when we build the mesh.
	// So instead, we compute which sides occlude which for every voxel type,
	// and generate culling masks ahead of time, using an approximation.
	// It may have a limitation of the number of different side types,
	// so it's a tradeoff to take when designing the models.

	//	struct TypeAndSide {
	//		uint16_t type;
	//		uint16_t side;
	//	};

	struct Pattern {
		std::bitset<RASTER_SIZE * RASTER_SIZE> bitmap;
		// StdVector<TypeAndSide> occurrences;
	};

	StdVector<Pattern> patterns;
	uint32_t full_side_pattern_index = VoxelBlockyLibraryBase::NULL_INDEX;

	// Gather patterns for each model
	for (uint16_t type_id = 0; type_id < baked_data.models.size(); ++type_id) {
		BakedModel &model_data = baked_data.models[type_id];
		model_data.contributes_to_ao = true;

		// For each side
		for (uint16_t side = 0; side < Cube::SIDE_COUNT; ++side) {
			std::bitset<RASTER_SIZE * RASTER_SIZE> bitmap;

			if (model_data.fluid_index != VoxelBlockyModel::NULL_FLUID_INDEX) {
				// Fluids don't have per-model static geometry, but their culling rules are still similar to a cube.
				// There is never a side on the top, it's either slightly lower (not on the side) or culled by
				// neighboring water. But we still need to fill bits as if there was one, otherwise the bottom of water
				// voxels would not get culled when stacked with other water voxels
				// if (side != Cube::SIDE_POSITIVE_Y) {
				bitmap.set();
				// }
				// Fluids don't contribute to AO for now
				model_data.contributes_to_ao = false;
			} else {
				rasterize_side_all_surfaces(model_data, side, bitmap);
			}

			{
				std::bitset<RASTER_SIZE * RASTER_SIZE> full_bitmap;
				full_bitmap.set();

				if ((bitmap & full_bitmap) == full_bitmap) {
					model_data.model.full_sides_mask |= (1 << side);
				}
			}

			// Find if the same pattern already exists
			uint32_t pattern_index = VoxelBlockyLibraryBase::NULL_INDEX;
			for (unsigned int i = 0; i < patterns.size(); ++i) {
				if (patterns[i].bitmap == bitmap) {
					pattern_index = i;
					break;
				}
			}

			// Get or create pattern
			Pattern *pattern = nullptr;
			if (pattern_index != VoxelBlockyLibraryBase::NULL_INDEX) {
				pattern = &patterns[pattern_index];
			} else {
				pattern_index = patterns.size();
				patterns.push_back(Pattern());
				pattern = &patterns.back();
				pattern->bitmap = bitmap;
			}

			CRASH_COND(pattern == nullptr);

			if (full_side_pattern_index == VoxelBlockyLibraryBase::NULL_INDEX && bitmap.all()) {
				full_side_pattern_index = pattern_index;
			}
			if (pattern_index != full_side_pattern_index) {
				// Non-cube voxels don't contribute to AO at the moment
				model_data.contributes_to_ao = false;
			}

			// pattern->occurrences.push_back(TypeAndSide{ type_id, side });
			model_data.model.side_pattern_indices[side] = pattern_index;

		} // side
	} // type

	// Find which pattern occludes which

	baked_data.side_pattern_count = patterns.size();
	baked_data.side_pattern_culling.resize_no_init(baked_data.side_pattern_count * baked_data.side_pattern_count);
	baked_data.side_pattern_culling.fill(false);

	for (unsigned int ai = 0; ai < patterns.size(); ++ai) {
		const Pattern &pattern_a = patterns[ai];

		if (pattern_a.bitmap.any()) {
			// Pattern always occludes itself
			baked_data.side_pattern_culling.set(ai + ai * baked_data.side_pattern_count);
		}

		for (unsigned int bi = ai + 1; bi < patterns.size(); ++bi) {
			const Pattern &pattern_b = patterns[bi];

			std::bitset<RASTER_SIZE * RASTER_SIZE> res = pattern_a.bitmap & pattern_b.bitmap;

			if (!res.any()) {
				// Patterns have nothing in common, there is no occlusion
				continue;
			}

			bool b_occludes_a = (res == pattern_a.bitmap);
			bool a_occludes_b = (res == pattern_b.bitmap);

			// Same patterns? That can't be, they must be unique
			CRASH_COND(b_occludes_a && a_occludes_b);

			if (a_occludes_b) {
				baked_data.side_pattern_culling.set(ai + bi * baked_data.side_pattern_count);

			} else if (b_occludes_a) {
				baked_data.side_pattern_culling.set(bi + ai * baked_data.side_pattern_count);
			}
		}
	}

	generate_library_cutout_sides(baked_data);

	// DEBUG
	/*print_line("");
	print_line("Side culling matrix");
	print_line("-------------------------");
	for (unsigned int i = 0; i < _side_pattern_count; ++i) {
		const Pattern &p = patterns[i];
		String line = String("[{0}] - ").format(varray(i));
		for (unsigned int j = 0; j < p.occurrences.size(); ++j) {
			TypeAndSide ts = p.occurrences[j];
			line += String("T{0}-{1} ").format(varray(ts.type, ts.side));
		}
		print_line(line);

		Ref<Image> im;
		im.instance();
		im->create(RASTER_SIZE, RASTER_SIZE, false, Image::FORMAT_RGB8);
		im->lock();
		for (int y = 0; y < RASTER_SIZE; ++y) {
			for (int x = 0; x < RASTER_SIZE; ++x) {
				if (p.bitmap.test(x + y * RASTER_SIZE)) {
					im->set_pixel(x, y, Color(1, 1, 1));
				} else {
					im->set_pixel(x, y, Color(0, 0, 0));
				}
			}
		}
		im->unlock();
		im->save_png(line + ".png");
	}
	{
		unsigned int i = 0;
		for (unsigned int bi = 0; bi < _side_pattern_count; ++bi) {
			String line;
			for (unsigned int ai = 0; ai < _side_pattern_count; ++ai, ++i) {
				if (_side_pattern_culling.get(i)) {
					line += "o ";
				} else {
					line += "- ";
				}
			}
			print_line(line);
		}
	}
	print_line("");*/
}

} // namespace blocky

} // namespace zylann::voxel
