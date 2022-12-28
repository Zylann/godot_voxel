#ifndef VOXEL_BLOCKY_LIBRARY_H
#define VOXEL_BLOCKY_LIBRARY_H

#include "../../util/dynamic_bitset.h"
#include "../../util/godot/classes/ref_counted.h"
#include "../../util/thread/rw_lock.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Stores a list of models that can be used with VoxelMesherBlocky
class VoxelBlockyLibrary : public Resource {
	GDCLASS(VoxelBlockyLibrary, Resource)

public:
	// Limit based on maximum supported by VoxelMesherBlocky
	static const unsigned int MAX_VOXEL_TYPES = 65536;
	static const uint32_t NULL_INDEX = 0xFFFFFFFF;

	struct BakedData {
		// 2D array: { X : pattern A, Y : pattern B } => Does A occlude B
		// Where index is X + Y * pattern count
		DynamicBitset side_pattern_culling;
		unsigned int side_pattern_count = 0;
		// Lots of data can get moved but it's only on load.
		std::vector<VoxelBlockyModel::BakedData> models;

		unsigned int indexed_materials_count = 0;

		inline bool has_model(uint32_t i) const {
			return i < models.size();
		}

		inline bool get_side_pattern_occlusion(unsigned int pattern_a, unsigned int pattern_b) const {
#ifdef DEBUG_ENABLED
			CRASH_COND(pattern_a >= side_pattern_count);
			CRASH_COND(pattern_b >= side_pattern_count);
#endif
			return side_pattern_culling.get(pattern_a + pattern_b * side_pattern_count);
		}
	};

	VoxelBlockyLibrary();
	~VoxelBlockyLibrary();

	int get_atlas_size() const {
		return _atlas_size;
	}
	void set_atlas_size(int s);

	// Use this factory rather than creating voxels from scratch
	Ref<VoxelBlockyModel> create_voxel(unsigned int id, String name);

	unsigned int get_voxel_count() const;
	void set_voxel_count(unsigned int type_count);

	bool get_bake_tangents() const {
		return _bake_tangents;
	}
	void set_bake_tangents(bool bt);

	void load_default();

	int get_voxel_index_from_name(StringName name) const;

	void bake();

	//-------------------------
	// Internal use

	inline bool has_voxel(unsigned int id) const {
		return id < _voxel_types.size() && _voxel_types[id].is_valid();
	}

	inline const VoxelBlockyModel &get_voxel_const(unsigned int id) const {
		const Ref<VoxelBlockyModel> &model = _voxel_types[id];
		ZN_ASSERT(model.is_valid());
		return **model;
	}

	const BakedData &get_baked_data() const {
		return _baked_data;
	}
	const RWLock &get_baked_data_rw_lock() const {
		return _baked_data_rw_lock;
	}

	Ref<Material> get_material_by_index(unsigned int index) const;

private:
	void set_voxel(unsigned int id, Ref<VoxelBlockyModel> voxel);

	void generate_side_culling_matrix();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	Ref<VoxelBlockyModel> _b_get_voxel(unsigned int id);
	Ref<VoxelBlockyModel> _b_get_voxel_by_name(StringName name);
	// Convenience method to get all indexed materials after baking,
	// which can be passed to VoxelMesher::build for testing
	TypedArray<Material> _b_get_materials() const;

	static void _bind_methods();

private:
	// There can be null entries. A vector is used because there should be no more than 65,536 items,
	// and in practice the intented use case rarely goes over a few hundreds
	std::vector<Ref<VoxelBlockyModel>> _voxel_types;
	int _atlas_size = 16;
	bool _needs_baking = true;
	bool _bake_tangents = true;

	// Used in multithread context by the mesher. Don't modify that outside of bake().
	RWLock _baked_data_rw_lock;
	BakedData _baked_data;
	// One of the entries can be null to represent "The default material". If all non-empty models have materials, there
	// won't be a null entry.
	std::vector<Ref<Material>> _indexed_materials;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_LIBRARY_H
