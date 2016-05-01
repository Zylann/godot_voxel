#ifndef VOXEL_MESH_BUILDER
#define VOXEL_MESH_BUILDER

#include <reference.h>
#include <scene/resources/mesh.h>
#include <scene/resources/surface_tool.h>
#include "voxel.h"
#include "voxel_buffer.h"

class VoxelMeshBuilder : public Reference {
    OBJ_TYPE(VoxelMeshBuilder, Reference);

    static const unsigned int MAX_VOXEL_TYPES = 256; // Required limit because voxel types are stored in 8 bits
    static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.

    Ref<Voxel> _voxel_types[MAX_VOXEL_TYPES];
    Ref<Material> _materials[MAX_MATERIALS];
    SurfaceTool _surface_tool[MAX_MATERIALS];

public:

    VoxelMeshBuilder();

    void add_voxel_type(Ref<Voxel> voxel);
    void set_material(Ref<Material> material, unsigned int id);

    Ref<Mesh> build(Ref<VoxelBuffer> buffer_ref);
    
protected:
    static void _bind_methods();

};


#endif // VOXEL_MESH_BUILDER
