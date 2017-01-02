#ifndef VOXEL_TYPE_H
#define VOXEL_TYPE_H

#include <reference.h>

class VoxelLibrary;

// Definition of one type of voxel.
// A voxel can be a simple coloured cube, or a more complex model.
// Important: it is recommended that you create voxels from a library rather than using new().
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

	Voxel();

	// Properties

	Ref<Voxel> set_name(String name);
	_FORCE_INLINE_ String get_name() const { return _name; }

	Ref<Voxel> set_id(int id);
	_FORCE_INLINE_ int get_id() const { return _id; }

	Ref<Voxel> set_color(Color color);
	_FORCE_INLINE_ Color get_color() const { return _color; }

	Ref<Voxel> set_material_id(unsigned int id);
	_FORCE_INLINE_ unsigned int get_material_id() const { return _material_id; }

	Ref<Voxel> set_transparent(bool t = true);
	_FORCE_INLINE_ bool is_transparent() const { return _is_transparent; }

	// Built-in geometry generators

	Ref<Voxel> set_cube_geometry(float sy = 1);
	Ref<Voxel> set_cube_uv_all_sides(Vector2 atlas_pos);
	Ref<Voxel> set_cube_uv_tbs_sides(Vector2 top_atlas_pos, Vector2 side_atlas_pos, Vector2 bottom_atlas_pos);
	//Ref<Voxel> set_xquad_geometry(Vector2 atlas_pos);

	// Getters for native usage only

	const DVector<Vector3> & get_model_vertices() const { return _model_vertices; }
	const DVector<Vector3> & get_model_normals() const { return _model_normals; }
	const DVector<Vector2> & get_model_uv() const { return _model_uv; }
	const DVector<Vector3> & get_model_side_vertices(unsigned int side) const { return _model_side_vertices[side]; }
	const DVector<Vector2> & get_model_side_uv(unsigned int side) const { return _model_side_uv[side]; }

	void set_library_ptr(VoxelLibrary * lib) { _library = lib; }

protected:
	Ref<Voxel> _set_cube_uv_sides(const Vector2 atlas_pos[6]);

	static void _bind_methods();

private:
	VoxelLibrary * _library;

	// Identifiers
	int _id;
	String _name;

	// Properties
	int _material_id;
	bool _is_transparent;

	// Model
	Color _color;
	DVector<Vector3> _model_vertices;
	DVector<Vector3> _model_normals;
	DVector<Vector2> _model_uv;
	DVector<Vector3> _model_side_vertices[SIDE_COUNT];
	DVector<Vector2> _model_side_uv[SIDE_COUNT];

	// TODO Child voxel types

};

#endif // VOXEL_TYPE_H

