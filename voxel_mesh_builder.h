#ifndef VOXEL_MESH_BUILDER
#define VOXEL_MESH_BUILDER

#include <reference.h>
#include <scene/resources/mesh.h>
#include <scene/resources/surface_tool.h>
#include "voxel.h"
#include "voxel_buffer.h"

class VoxelLibrary;

class VoxelMeshBuilder : public Reference {
    OBJ_TYPE(VoxelMeshBuilder, Reference);

public:
    static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.

private:
    Ref<VoxelLibrary> _library;
    Ref<Material> _materials[MAX_MATERIALS];
    SurfaceTool _surface_tool[MAX_MATERIALS];

public:
    VoxelMeshBuilder();

    void set_material(Ref<Material> material, unsigned int id);

    void set_library(Ref<VoxelLibrary> library);

    Ref<Mesh> build(Ref<VoxelBuffer> buffer_ref);
    
protected:
    static void _bind_methods();

};


#endif // VOXEL_MESH_BUILDER
