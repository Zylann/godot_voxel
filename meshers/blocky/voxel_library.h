#ifndef VOXEL_LIBRARY_H
#define VOXEL_LIBRARY_H

#include "../../util/dynamic_bitset.h"
#include "voxel.h"
#include <core/resource.h>

class VoxelLibrary : public Resource {
	GDCLASS(VoxelLibrary, Resource)

public:
	// Limit based on maximum supported by VoxelMesherBlocky
	static const unsigned int MAX_VOXEL_TYPES = 65536;

	VoxelLibrary();
	~VoxelLibrary();

	int get_atlas_size() const { return _atlas_size; }
	void set_atlas_size(int s);

	// Use this factory rather than creating voxels from scratch
	Ref<Voxel> create_voxel(unsigned int id, String name);

	unsigned int get_voxel_count() const;
	void set_voxel_count(unsigned int type_count);

	void load_default();

	// Internal getters

	_FORCE_INLINE_ bool has_voxel(unsigned int id) const {
		return id < _voxel_types.size() && _voxel_types[id].is_valid();
	}

	_FORCE_INLINE_ const Voxel &get_voxel_const(unsigned int id) const {
		return **_voxel_types[id];
	}

	void bake();

	bool get_side_pattern_occlusion(unsigned int pattern_a, unsigned int pattern_b) const;

private:
	void generate_side_culling_matrix();

	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	Ref<Voxel> _b_get_voxel(unsigned int id);

private:
	std::vector<Ref<Voxel> > _voxel_types;
	int _atlas_size;

	// 2D array: { X : pattern A, Y : pattern B } => Does A occlude B
	// Where index is X + Y * pattern count
	DynamicBitset _side_pattern_culling;
	unsigned int _side_pattern_count = 0;
};

#endif // VOXEL_LIBRARY_H
