#ifndef VOXEL_TYPE_H
#define VOXEL_TYPE_H

#include <reference.h>

// Definition of one type of voxel.
// A voxel can be a simple coloured cube, or a more complex model.
class Voxel : public Reference {
    OBJ_TYPE(Voxel, Reference)

public:
    enum Side {
        SIDE_LEFT = 0,
        SIDE_RIGHT,
        SIDE_BOTTOM,
        SIDE_TOP,
        SIDE_BACK,
        SIDE_FRONT,

        SIDE_COUNT
    };

private:
    int _id;
    String _name;
    int _material_id;
    bool _is_transparent;

    Color _color;
    DVector<Vector3> _model_vertices;
    DVector<Vector3> _model_normals;
    DVector<Vector2> _model_uv;
    DVector<Vector3> _model_side_vertices[SIDE_COUNT];
    DVector<Vector2> _model_side_uv[SIDE_COUNT];

    // TODO Child voxel types

public:

    Voxel();

    _FORCE_INLINE_ void set_name(String name) { _name = name; }
    _FORCE_INLINE_ String get_name() const { return _name; }

    void set_id(int id);
    _FORCE_INLINE_ int get_id() const { return _id; }

    _FORCE_INLINE_ void set_color(Color color) { _color = color; }
    _FORCE_INLINE_ Color get_color() const { return _color; }

    _FORCE_INLINE_ void set_material_id(unsigned int id) { _material_id = id; }
    _FORCE_INLINE_ unsigned int get_material_id() const { return _material_id; }

    void set_cube_geometry(float sy = 1);
    void set_cube_uv_all_sides(Vector3 atlas_pos);
    void set_cube_uv_tbs_sides(Vector3 top_atlas_pos, Vector3 side_atlas_pos, Vector3 bottom_atlas_pos);

    const DVector<Vector3> & get_model_vertices() const { return _model_vertices; }
    const DVector<Vector3> & get_model_normals() const { return _model_normals; }
    const DVector<Vector2> & get_model_uv() const { return _model_uv; }
    const DVector<Vector3> & get_model_side_vertices(unsigned int side) const { return _model_side_vertices[side]; }
    const DVector<Vector2> & get_model_side_uv(unsigned int side) const { return _model_side_uv[side]; }

protected:
    static void _bind_methods();

};

#endif // VOXEL_TYPE_H

