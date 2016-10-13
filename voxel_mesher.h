#ifndef VOXEL_MESHER
#define VOXEL_MESHER

#include <reference.h>
#include <scene/resources/mesh.h>
#include <scene/resources/surface_tool.h>
#include "voxel.h"
#include "voxel_buffer.h"
#include "voxel_library.h"

class VoxelMesher : public Reference {
    OBJ_TYPE(VoxelMesher, Reference);

public:
    static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.

private:
    Ref<VoxelLibrary> _library;
    Ref<Material> _materials[MAX_MATERIALS];
    SurfaceTool _surface_tool[MAX_MATERIALS];
    float _baked_occlusion_darkness;
    bool _bake_occlusion;

public:
    VoxelMesher();

    void set_material(Ref<Material> material, unsigned int id);

    void set_library(Ref<VoxelLibrary> library);

    void set_occlusion_darkness(float darkness);
    
    void set_occlusion_enabled(bool enable);

    Ref<Mesh> build(const VoxelBuffer & buffer_ref);
    Ref<Mesh> build_ref(Ref<VoxelBuffer> buffer_ref);
    
protected:

    static void _bind_methods();

};


#endif // VOXEL_MESHER
