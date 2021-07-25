#ifndef VOXEL_LIBRARY_H
#define VOXEL_LIBRARY_H

#include "../../util/dynamic_bitset.h"
#include "voxel.h"
#include <core/resource.h>

// TODO Rename VoxelBlockyLibrary

// Stores a list of models that can be used with VoxelMesherBlocky
class VoxelLibrary : public Resource {
	GDCLASS(VoxelLibrary, Resource)

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
		std::vector<Voxel::BakedData> models;

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

	VoxelLibrary();
	~VoxelLibrary();

	int get_atlas_size() const { return _atlas_size; }
	void set_atlas_size(int s);

	// Use this factory rather than creating voxels from scratch
	Ref<Voxel> create_voxel(unsigned int id, String name);

	unsigned int get_voxel_count() const;
	void set_voxel_count(unsigned int type_count);

	bool get_bake_tangents() const { return _bake_tangents; }
	void set_bake_tangents(bool bt);

	void load_default();

	int get_voxel_index_from_name(StringName name) const;

	void bake();

	//-------------------------
	// Internal use

	_FORCE_INLINE_ bool has_voxel(unsigned int id) const {
		return id < _voxel_types.size() && _voxel_types[id].is_valid();
	}

	_FORCE_INLINE_ const Voxel &get_voxel_const(unsigned int id) const {
		return **_voxel_types[id];
	}

	const BakedData &get_baked_data() const { return _baked_data; }
	const RWLock &get_baked_data_rw_lock() const { return _baked_data_rw_lock; }

private:
	void set_voxel(unsigned int id, Ref<Voxel> voxel);

	void generate_side_culling_matrix();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	Ref<Voxel> _b_get_voxel(unsigned int id);
	Ref<Voxel> _b_get_voxel_by_name(StringName name);

	static void _bind_methods();

private:
	// There can be null entries. A vector is used because there should be no more than 65,536 items,
	// and in practice the intented use case rarely goes over a few hundreds
	std::vector<Ref<Voxel>> _voxel_types;
	int _atlas_size = 16;
	bool _needs_baking = true;
	bool _bake_tangents = true;

	// Used in multithread context by the mesher. Don't modify that outside of bake().
	RWLock _baked_data_rw_lock;
	BakedData _baked_data;
};

#endif // VOXEL_LIBRARY_H
