#include "voxel_blocky_library.h"
#include "../../util/godot/classes/time.h"
#include "../../util/log.h"
#include "../../util/math/triangle.h"
#include "../../util/string_funcs.h"

#include <bitset>

namespace zylann::voxel {

VoxelBlockyLibrary::VoxelBlockyLibrary() {}

VoxelBlockyLibrary::~VoxelBlockyLibrary() {}

unsigned int VoxelBlockyLibrary::get_voxel_count() const {
	return _voxel_types.size();
}

void VoxelBlockyLibrary::set_voxel_count(unsigned int type_count) {
	ERR_FAIL_COND(type_count > MAX_VOXEL_TYPES);
	if (type_count == _voxel_types.size()) {
		return;
	}
	// Note: a smaller size may cause a loss of data
	_voxel_types.resize(type_count);
	notify_property_list_changed();
	_needs_baking = true;
}

int VoxelBlockyLibrary::get_voxel_index_from_name(StringName name) const {
	for (size_t i = 0; i < _voxel_types.size(); ++i) {
		const Ref<VoxelBlockyModel> &v = _voxel_types[i];
		if (v.is_null()) {
			continue;
		}
		if (v->get_voxel_name() == name) {
			return i;
		}
	}
	return -1;
}

void VoxelBlockyLibrary::load_default() {
	set_voxel_count(2);

	Ref<VoxelBlockyModel> air = create_voxel(0, "air");
	air->set_transparent(true);

	Ref<VoxelBlockyModel> solid = create_voxel(1, "solid");
	solid->set_transparent(false);
	solid->set_geometry_type(VoxelBlockyModel::GEOMETRY_CUBE);
}

// TODO Add a way to add voxels

bool VoxelBlockyLibrary::_set(const StringName &p_name, const Variant &p_value) {
	//	if(p_name == "voxels/max") {

	//		int v = p_value;
	//		_max_count = CLAMP(v, 0, MAX_VOXEL_TYPES);
	//		for(int i = _max_count; i < MAX_VOXEL_TYPES; ++i) {
	//			_voxel_types[i] = Ref<Voxel>();
	//			return true;
	//		}

	//	} else
	String name(p_name);
	if (name.begins_with("voxels/")) {
		unsigned int idx = name.get_slicec('/', 1).to_int();
		set_voxel(idx, p_value);
		return true;
	}

	return false;
}

bool VoxelBlockyLibrary::_get(const StringName &p_name, Variant &r_ret) const {
	//	if(p_name == "voxels/max") {

	//		r_ret = _max_count;
	//		return true;

	//	} else
	String name(p_name);
	if (name.begins_with("voxels/")) {
		const unsigned int idx = name.get_slicec('/', 1).to_int();
		if (idx < _voxel_types.size()) {
			r_ret = _voxel_types[idx];
			return true;
		}
	}

	return false;
}

void VoxelBlockyLibrary::_get_property_list(List<PropertyInfo> *p_list) const {
	for (unsigned int i = 0; i < _voxel_types.size(); ++i) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, String("voxels/") + itos(i), PROPERTY_HINT_RESOURCE_TYPE,
				VoxelBlockyModel::get_class_static()));
	}
}

void VoxelBlockyLibrary::set_atlas_size(int s) {
	ERR_FAIL_COND(s <= 0);
	_atlas_size = s;
	_needs_baking = true;
}

void VoxelBlockyLibrary::set_bake_tangents(bool bt) {
	_bake_tangents = bt;
	_needs_baking = true;
}

Ref<VoxelBlockyModel> VoxelBlockyLibrary::create_voxel(unsigned int id, String name) {
	ERR_FAIL_COND_V(id >= _voxel_types.size(), Ref<VoxelBlockyModel>());
	Ref<VoxelBlockyModel> voxel(memnew(VoxelBlockyModel));
	voxel->set_id(id);
	voxel->set_voxel_name(name);
	_voxel_types[id] = voxel;
	return voxel;
}

void VoxelBlockyLibrary::set_voxel(unsigned int idx, Ref<VoxelBlockyModel> voxel) {
	ERR_FAIL_INDEX(idx, MAX_VOXEL_TYPES);

	if (idx >= _voxel_types.size()) {
		_voxel_types.resize(idx + 1);
	}

	_voxel_types[idx] = voxel;
	if (voxel.is_valid()) {
		voxel->set_id(idx);
	}

	_needs_baking = true;
}

template <typename F>
static void rasterize_triangle_barycentric(Vector2f a, Vector2f b, Vector2f c, F output_func) {
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

void VoxelBlockyLibrary::bake() {
	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	VoxelBlockyModel::MaterialIndexer materials{ _indexed_materials };

	_baked_data.models.resize(_voxel_types.size());
	for (size_t i = 0; i < _voxel_types.size(); ++i) {
		Ref<VoxelBlockyModel> config = _voxel_types[i];
		if (config.is_valid()) {
			_voxel_types[i]->bake(_baked_data.models[i], _atlas_size, _bake_tangents, materials);
		} else {
			_baked_data.models[i].clear();
		}
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	generate_side_culling_matrix();

	uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake VoxelLibrary, indexed {} materials", time_spent, _indexed_materials.size()));
}

void VoxelBlockyLibrary::generate_side_culling_matrix() {
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
		// std::vector<TypeAndSide> occurrences;
	};

	std::vector<Pattern> patterns;
	uint32_t full_side_pattern_index = NULL_INDEX;

	CRASH_COND(_voxel_types.size() != _baked_data.models.size());

	// Gather patterns for each model
	for (uint16_t type_id = 0; type_id < _voxel_types.size(); ++type_id) {
		if (_voxel_types[type_id].is_null()) {
			continue;
		}

		VoxelBlockyModel::BakedData &model_data = _baked_data.models[type_id];
		model_data.contributes_to_ao = true;

		// For each side
		for (uint16_t side = 0; side < Cube::SIDE_COUNT; ++side) {
			std::bitset<RASTER_SIZE * RASTER_SIZE> bitmap;

			// For each surface (they are all combined for simplicity, though it is also a limitation)
			for (unsigned int surface_index = 0; surface_index < model_data.model.surface_count; ++surface_index) {
				const VoxelBlockyModel::BakedData::Surface &surface = model_data.model.surfaces[surface_index];

				const std::vector<Vector3f> &positions = surface.side_positions[side];
				const std::vector<int> &indices = surface.side_indices[side];
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
			uint32_t pattern_index = NULL_INDEX;
			for (unsigned int i = 0; i < patterns.size(); ++i) {
				if (patterns[i].bitmap == bitmap) {
					pattern_index = i;
					break;
				}
			}

			// Get or create pattern
			Pattern *pattern = nullptr;
			if (pattern_index != NULL_INDEX) {
				pattern = &patterns[pattern_index];
			} else {
				pattern_index = patterns.size();
				patterns.push_back(Pattern());
				pattern = &patterns.back();
				pattern->bitmap = bitmap;
			}

			CRASH_COND(pattern == nullptr);

			if (full_side_pattern_index == NULL_INDEX && bitmap.all()) {
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

	_baked_data.side_pattern_count = patterns.size();
	_baked_data.side_pattern_culling.resize(_baked_data.side_pattern_count * _baked_data.side_pattern_count);
	_baked_data.side_pattern_culling.fill(false);

	for (unsigned int ai = 0; ai < patterns.size(); ++ai) {
		const Pattern &pattern_a = patterns[ai];

		if (pattern_a.bitmap.any()) {
			// Pattern always occludes itself
			_baked_data.side_pattern_culling.set(ai + ai * _baked_data.side_pattern_count);
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
				_baked_data.side_pattern_culling.set(ai + bi * _baked_data.side_pattern_count);

			} else if (b_occludes_a) {
				_baked_data.side_pattern_culling.set(bi + ai * _baked_data.side_pattern_count);
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

Ref<Material> VoxelBlockyLibrary::get_material_by_index(unsigned int index) const {
	ERR_FAIL_INDEX_V(index, _indexed_materials.size(), Ref<Material>());
	return _indexed_materials[index];
}

Ref<VoxelBlockyModel> VoxelBlockyLibrary::_b_get_voxel(unsigned int id) {
	ERR_FAIL_COND_V(id >= _voxel_types.size(), Ref<VoxelBlockyModel>());
	return _voxel_types[id];
}

Ref<VoxelBlockyModel> VoxelBlockyLibrary::_b_get_voxel_by_name(StringName name) {
	int id = get_voxel_index_from_name(name);
	ERR_FAIL_COND_V(id == -1, Ref<VoxelBlockyModel>());
	return _voxel_types[id];
}

TypedArray<Material> VoxelBlockyLibrary::_b_get_materials() const {
	// Note, if at least one non-empty voxel has no material, there will be one null entry in this list to represent
	// "The default material".
	TypedArray<Material> materials;
	materials.resize(_indexed_materials.size());
	for (size_t i = 0; i < _indexed_materials.size(); ++i) {
		materials[i] = _indexed_materials[i];
	}
	return materials;
}

void VoxelBlockyLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_voxel", "id", "name"), &VoxelBlockyLibrary::create_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel", "id"), &VoxelBlockyLibrary::_b_get_voxel);

	ClassDB::bind_method(D_METHOD("set_atlas_size", "square_size"), &VoxelBlockyLibrary::set_atlas_size);
	ClassDB::bind_method(D_METHOD("get_atlas_size"), &VoxelBlockyLibrary::get_atlas_size);

	ClassDB::bind_method(D_METHOD("set_voxel_count", "count"), &VoxelBlockyLibrary::set_voxel_count);
	ClassDB::bind_method(D_METHOD("get_voxel_count"), &VoxelBlockyLibrary::get_voxel_count);

	ClassDB::bind_method(D_METHOD("get_bake_tangents"), &VoxelBlockyLibrary::get_bake_tangents);
	ClassDB::bind_method(D_METHOD("set_bake_tangents", "bake_tangents"), &VoxelBlockyLibrary::set_bake_tangents);

	ClassDB::bind_method(D_METHOD("get_voxel_index_from_name", "name"), &VoxelBlockyLibrary::get_voxel_index_from_name);
	ClassDB::bind_method(D_METHOD("get_voxel_by_name", "name"), &VoxelBlockyLibrary::_b_get_voxel_by_name);

	ClassDB::bind_method(D_METHOD("bake"), &VoxelBlockyLibrary::bake);

	ClassDB::bind_method(D_METHOD("get_materials"), &VoxelBlockyLibrary::_b_get_materials);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "atlas_size"), "set_atlas_size", "get_atlas_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "voxel_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR),
			"set_voxel_count", "get_voxel_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_tangents"), "set_bake_tangents", "get_bake_tangents");

	BIND_CONSTANT(MAX_VOXEL_TYPES);
}

} // namespace zylann::voxel
