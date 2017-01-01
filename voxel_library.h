#ifndef VOXEL_LIBRARY_H
#define VOXEL_LIBRARY_H

#include <reference.h>
#include "voxel.h"

class VoxelLibrary : public Reference {
	OBJ_TYPE(VoxelLibrary, Reference)

public:
	static const unsigned int MAX_VOXEL_TYPES = 256; // Required limit because voxel types are stored in 8 bits

	VoxelLibrary();
	~VoxelLibrary();

	int get_atlas_size() const { return _atlas_size; }
	void set_atlas_size(int s);

	// Use this factory rather than creating voxels from scratch
	Ref<Voxel> create_voxel(int id, String name);

	// Internal getters

	_FORCE_INLINE_ bool has_voxel(int id) const { return _voxel_types[id].is_valid(); }
	_FORCE_INLINE_ const Voxel & get_voxel_const(int id) const { return **_voxel_types[id]; }

protected:
	static void _bind_methods();

	Ref<Voxel> _get_voxel_bind(int id);

private:
	Ref<Voxel> _voxel_types[MAX_VOXEL_TYPES];
	int _atlas_size;

};

#endif // VOXEL_LIBRARY_H

