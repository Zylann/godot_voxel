#include "voxel.h"

Voxel::Voxel() : Reference(), _id(0), _material_id(0), _is_transparent(false), _color(1.f, 1.f, 1.f) {

}

void Voxel::set_id(int id) {
    ERR_FAIL_COND(id < 0 || id >= 256);
    _id = id;
}

void Voxel::set_cube_geometry(float sy) {
    const Vector3 vertices[SIDE_COUNT][6] = {
        {
            // LEFT
            Vector3(0, 0, 0),
            Vector3(0, sy, 0),
            Vector3(0, sy, 1),
            Vector3(0, 0, 0),
            Vector3(0, sy, 1),
            Vector3(0, 0, 1),
        },
        {
            // RIGHT
            Vector3(1, 0, 0),
            Vector3(1, sy, 1),
            Vector3(1, sy, 0),
            Vector3(1, 0, 0),
            Vector3(1, 0, 1),
            Vector3(1, sy, 1)
        },
        {
            // BOTTOM
            Vector3(0, 0, 0),
            Vector3(1, 0, 1),
            Vector3(1, 0, 0),
            Vector3(0, 0, 0),
            Vector3(0, 0, 1),
            Vector3(1, 0, 1)
        },
        {
            // TOP
            Vector3(0, sy, 0),
            Vector3(1, sy, 0),
            Vector3(1, sy, 1),
            Vector3(0, sy, 0),
            Vector3(1, sy, 1),
            Vector3(0, sy, 1)
        },
        {
            // BACK
            Vector3(0, 0, 0),
            Vector3(1, 0, 0),
            Vector3(1, sy, 0),
            Vector3(0, 0, 0),
            Vector3(1, sy, 0),
            Vector3(0, sy, 0),
        },
        {
            // FRONT
            Vector3(1, 0, 1),
            Vector3(0, 0, 1),
            Vector3(1, sy, 1),
            Vector3(0, 0, 1),
            Vector3(0, sy, 1),
            Vector3(1, sy, 1)
        }
    };

    for (unsigned int side = 0; side < SIDE_COUNT; ++side) {
        _model_side_vertices[side].resize(6);
        DVector<Vector3>::Write w = _model_side_vertices[side].write();
        for (unsigned int i = 0; i < 6; ++i) {
            w[i] = vertices[side][i];
        }
    }
}

void Voxel::set_cube_uv_all_sides(Vector3 atlas_pos) {
    // TODO
}

void Voxel::set_cube_uv_tbs_sides(Vector3 top_atlas_pos, Vector3 side_atlas_pos, Vector3 bottom_atlas_pos) {
    // TODO
}

void Voxel::_bind_methods() {

    ObjectTypeDB::bind_method(_MD("set_name", "name"), &Voxel::set_name);
    ObjectTypeDB::bind_method(_MD("get_name"), &Voxel::get_name);

    ObjectTypeDB::bind_method(_MD("set_id", "id"), &Voxel::set_id);
    ObjectTypeDB::bind_method(_MD("get_id"), &Voxel::get_id);

    ObjectTypeDB::bind_method(_MD("set_color", "color"), &Voxel::set_color);
    ObjectTypeDB::bind_method(_MD("get_color"), &Voxel::get_color);

    ObjectTypeDB::bind_method(_MD("set_cube_geometry", "height"), &Voxel::set_cube_geometry, DEFVAL(1.f));
    // TODO

}

