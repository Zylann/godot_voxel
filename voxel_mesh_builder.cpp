#include "voxel_mesh_builder.h"

static const Vector3i g_side_normals[Voxel::SIDE_COUNT] = {
    Vector3i(-1, 0, 0),
    Vector3i(1, 0, 0),
    Vector3i(0, -1, 0),
    Vector3i(0, 1, 0),
    Vector3i(0, 0, -1),
    Vector3i(0, 0, 1),
};

VoxelMeshBuilder::VoxelMeshBuilder() {

}

void VoxelMeshBuilder::add_voxel_type(Ref<Voxel> voxel) {
    ERR_FAIL_COND(voxel.is_null());
    ERR_FAIL_COND(voxel->get_id() >= MAX_VOXEL_TYPES);
    ERR_FAIL_COND(voxel->get_material_id() >= MAX_MATERIALS);
    unsigned int id = voxel->get_id();
    _voxel_types[id] = voxel;
}

void VoxelMeshBuilder::set_material(Ref<Material> material, unsigned int id) {
    ERR_FAIL_COND(id >= MAX_MATERIALS);
    _materials[id] = material;
    _surface_tool[id].set_material(material);
}

Ref<Mesh> VoxelMeshBuilder::build(Ref<VoxelBuffer> buffer_ref) {
    ERR_FAIL_COND_V(buffer_ref.is_null(), Ref<Mesh>());

    const VoxelBuffer & buffer = **buffer_ref;

    for (unsigned int i = 0; i < MAX_MATERIALS; ++i) {
        _surface_tool[i].begin(Mesh::PRIMITIVE_TRIANGLES);
    }

    // Iterate 3D padded data to extract voxel faces.
    // This is the most intensive job in this class, so all required data should be as fit as possible.
    for (unsigned int z = 1; z < buffer.get_size_z()-1; ++z) {
        for (unsigned int x = 1; x < buffer.get_size_x()-1; ++x) {
            for (unsigned int y = 1; y < buffer.get_size_y()-1; ++y) {

                int voxel_id = buffer.get_voxel_local(x, y, z, 0);

                if (voxel_id != 0 && !_voxel_types[voxel_id].is_null()) {

                    const Voxel & voxel = **_voxel_types[voxel_id];

                    SurfaceTool & st = _surface_tool[voxel.get_material_id()];

                    // Hybrid approach: extract cube faces and decimate those that aren't visible,
                    // and still allow voxels to have geometry that is not a cube

                    // Sides
                    for (unsigned int side = 0; side < Voxel::SIDE_COUNT; ++side) {

                        const DVector<Vector3> & vertices = voxel.get_model_side_vertices(side);
                        if (vertices.size() != 0) {

                            Vector3i normal = g_side_normals[side];
                            unsigned nx = x + normal.x;
                            unsigned ny = y + normal.y;
                            unsigned nz = z + normal.z;

                            int neighbor_voxel_id = buffer.get_voxel_local(nx, ny, nz, 0);
                            // TODO Better face visibility test
                            if (neighbor_voxel_id == 0) {

                                DVector<Vector3>::Read r = vertices.read();
                                Vector3 pos(x - 1, y - 1, z - 1);

                                for (unsigned int i = 0; i < vertices.size(); ++i) {
                                    st.add_normal(Vector3(normal.x, normal.y, normal.z));
                                    st.add_vertex(r[i] + pos);
                                }
                            }
                        }
                    }

                    // Inside
                    if (voxel.get_model_vertices().size() != 0) {

                        const DVector<Vector3> & vertices = voxel.get_model_vertices();
                        DVector<Vector3>::Read rv = voxel.get_model_vertices().read();
                        DVector<Vector3>::Read rn = voxel.get_model_normals().read();
                        Vector3 pos(x - 1, y - 1, z - 1);

                        for (unsigned int i = 0; i < vertices.size(); ++i) {
                            st.add_normal(rn[i]);
                            st.add_vertex(rv[i] + pos);
                        }
                    }

                }

            }
        }
    }

    // Commit mesh
    Ref<Mesh> mesh_ref = _surface_tool[0].commit();
    _surface_tool[0].clear();
    for (unsigned int i = 1; i < MAX_MATERIALS; ++i) {
        if (_materials[i].is_valid()) {
            SurfaceTool & st = _surface_tool[i];
            st.commit(mesh_ref);
            st.clear();
        }
    }

    return mesh_ref;
}

void VoxelMeshBuilder::_bind_methods() {

    ObjectTypeDB::bind_method(_MD("add_voxel_type", "voxel"), &VoxelMeshBuilder::add_voxel_type);
    ObjectTypeDB::bind_method(_MD("set_material", "material", "id"), &VoxelMeshBuilder::set_material);
    ObjectTypeDB::bind_method(_MD("build", "voxel_buffer"), &VoxelMeshBuilder::build);

}

