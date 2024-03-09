#include "voxel_blocky_library_base.h"
#include "../../util/math/triangle.h"
#include "../../util/profiling.h"
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

template <typename F>
void rasterize_triangle_barycentric(Vector2f a, Vector2f b, Vector2f c, F output_func) {
	// Slower than scanline method, but looks better

	// Grow the triangle a tiny bit, to help against floating point error
	const Vector2f m = 0.333333f * (a + b + c);
	a += 0.001f * (a - m);
	b += 0.001f * (b - m);
	c += 0.001f * (c - m);

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

void generate_side_culling_matrix(VoxelBlockyLibraryBase::BakedData &baked_data) {
	ZN_PROFILE_SCOPE();
	// When two blocky voxels are next to each other, they share a side.
	// Geometry of either side can be culled away if covered by the other,
	// but it's very expensive to do a full polygon check when we build the mesh.
	// So instead, we compute which sides occlude which for every voxel type,
	// and generate culling masks ahead of time, using an approximation.
	// It may have a limitation of the number of different side types,
	// so it's a tradeoff to take when designing the models.

	static const unsigned int RASTER_SIZE = 32;

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
		VoxelBlockyModel::BakedData &model_data = baked_data.models[type_id];
		model_data.contributes_to_ao = true;

		// For each side
		for (uint16_t side = 0; side < Cube::SIDE_COUNT; ++side) {
			std::bitset<RASTER_SIZE * RASTER_SIZE> bitmap;

			// For each surface (they are all combined for simplicity, though it is also a limitation)
			for (unsigned int surface_index = 0; surface_index < model_data.model.surface_count; ++surface_index) {
				const VoxelBlockyModel::BakedData::Surface &surface = model_data.model.surfaces[surface_index];

				const StdVector<Vector3f> &positions = surface.side_positions[side];
				const StdVector<int> &indices = surface.side_indices[side];
				ERR_FAIL_COND(indices.size() % 3 != 0);

				// For each triangle
				for (unsigned int j = 0; j < indices.size(); j += 3) {
					const Vector3f va = positions[indices[j]];
					const Vector3f vb = positions[indices[j + 1]];
					const Vector3f vc = positions[indices[j + 2]];

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
	baked_data.side_pattern_culling.resize(baked_data.side_pattern_count * baked_data.side_pattern_count);
	baked_data.side_pattern_culling.fill(false);

	for (unsigned int ai = 0; ai < patterns.size(); ++ai) {
		const Pattern &pattern_a = patterns[ai];

		if (pattern_a.bitmap.any()) {
			// Pattern always occludes itself
			baked_data.side_pattern_culling.set(ai + ai * baked_data.side_pattern_count);
		}

		for (unsigned int bi = ai + 1; bi < patterns.size(); ++bi) {
			const Pattern &pattern_b = patterns[bi];

			std::bitset<RASTER_SIZE *RASTER_SIZE> res = pattern_a.bitmap & pattern_b.bitmap;

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

} // namespace zylann::voxel
