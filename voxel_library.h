#ifndef VOXEL_LIBRARY_H
#define VOXEL_LIBRARY_H

#include "voxel.h"
#include <core/resource.h>

class VoxelLibrary : public Resource {
	GDCLASS(VoxelLibrary, Resource)

public:
	static const unsigned int MAX_VOXEL_TYPES = 256; // Required limit because voxel types are stored in 8 bits

	VoxelLibrary();
	~VoxelLibrary();

	int get_atlas_size() const { return _atlas_size; }
	void set_atlas_size(int s);

	// Use this factory rather than creating voxels from scratch
	Ref<Voxel> create_voxel(unsigned int id, String name);

	int get_voxel_count() const;

	void load_default();

	// Internal getters

	_FORCE_INLINE_ bool has_voxel(int id) const { return _voxel_types[id].is_valid(); }
	_FORCE_INLINE_ const Voxel &get_voxel_const(int id) const { return **_voxel_types[id]; }

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	Ref<Voxel> _get_voxel_bind(unsigned int id);

private:
	Ref<Voxel> _voxel_types[MAX_VOXEL_TYPES];
	int _atlas_size;
};

#endif // VOXEL_LIBRARY_H
