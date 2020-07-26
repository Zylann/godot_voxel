#ifndef VOXEL_TYPE_H
#define VOXEL_TYPE_H

#include "../../cube_tables.h"
#include "../../util/fixed_array.h"
#include <core/resource.h>

class VoxelLibrary;

// TODO Rename VoxelBlockyType?
// Definition of one type of voxel.
// A voxel can be a simple coloured cube, or a more complex model.
// Important: it is recommended that you create voxels from a library rather than using new().
class Voxel : public Resource {
	GDCLASS(Voxel, Resource)

public:
	Voxel();

	enum Side {
		SIDE_NEGATIVE_X = Cube::SIDE_NEGATIVE_X,
		SIDE_POSITIVE_X = Cube::SIDE_POSITIVE_X,
		SIDE_NEGATIVE_Y = Cube::SIDE_NEGATIVE_Y,
		SIDE_POSITIVE_Y = Cube::SIDE_POSITIVE_Y,
		SIDE_NEGATIVE_Z = Cube::SIDE_NEGATIVE_Z,
		SIDE_POSITIVE_Z = Cube::SIDE_POSITIVE_Z,
		SIDE_COUNT = Cube::SIDE_COUNT
	};

	// Properties

	Ref<Voxel> set_voxel_name(String name);
	_FORCE_INLINE_ StringName get_voxel_name() const { return _name; }

	Ref<Voxel> set_id(int id);
	_FORCE_INLINE_ int get_id() const { return _id; }

	Ref<Voxel> set_color(Color color);
	_FORCE_INLINE_ Color get_color() const { return _color; }

	Ref<Voxel> set_material_id(unsigned int id);
	_FORCE_INLINE_ unsigned int get_material_id() const { return _material_id; }

	Ref<Voxel> set_transparent(bool t = true);
	_FORCE_INLINE_ bool is_transparent() const { return _is_transparent; }

	void set_custom_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_custom_mesh() const { return _custom_mesh; }

	//-------------------------------------------
	// Built-in geometry generators

	enum GeometryType {
		GEOMETRY_NONE = 0,
		GEOMETRY_CUBE,
		GEOMETRY_CUSTOM_MESH,
		GEOMETRY_MAX
	};

	void set_geometry_type(GeometryType type);
	GeometryType get_geometry_type() const;

	Ref<Resource> duplicate(bool p_subresources) const override;

	//------------------------------------------
	// Properties for native usage only

	const std::vector<Vector3> &get_model_positions() const { return _model_positions; }
	const std::vector<Vector3> &get_model_normals() const { return _model_normals; }
	const std::vector<Vector2> &get_model_uv() const { return _model_uvs; }
	const std::vector<int> &get_model_indices() const { return _model_indices; }

	const std::vector<Vector3> &get_model_side_positions(unsigned int side) const { return _model_side_positions[side]; }
	const std::vector<Vector2> &get_model_side_uv(unsigned int side) const { return _model_side_uvs[side]; }
	const std::vector<int> &get_model_side_indices(unsigned int side) const { return _model_side_indices[side]; }

	const std::vector<AABB> &get_collision_aabbs() const { return _collision_aabbs; }

	void set_library(Ref<VoxelLibrary> lib);

	void set_side_pattern_index(int side, uint32_t i);
	inline uint32_t get_side_pattern_index(int side) const { return _side_pattern_index[side]; }

	inline bool is_contributing_to_ao() const { return _contributes_to_ao; }
	inline void set_contributing_to_ao(bool b) { _contributes_to_ao = b; }

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void set_cube_uv_side(int side, Vector2 tile_pos);
	void update_cube_uv_sides();

	VoxelLibrary *get_library() const;

	static void _bind_methods();

	void clear_geometry();
	Ref<Voxel> set_cube_geometry(float sy = 1);

	Array _b_get_collision_aabbs() const;
	void _b_set_collision_aabbs(Array array);

private:
	ObjectID _library;

	// Identifiers
	int _id;
	StringName _name;

	// Properties
	int _material_id;
	bool _is_transparent;
	Color _color;
	GeometryType _geometry_type;
	float _cube_geometry_padding_y;
	FixedArray<Vector2, Cube::SIDE_COUNT> _cube_tiles;
	Ref<Mesh> _custom_mesh;
	std::vector<AABB> _collision_aabbs;
	bool _contributes_to_ao = false;

	FixedArray<uint32_t, Cube::SIDE_COUNT> _side_pattern_index;

	// Model
	std::vector<Vector3> _model_positions;
	std::vector<Vector3> _model_normals;
	std::vector<Vector2> _model_uvs;
	std::vector<int> _model_indices;
	// Model sides:
	// They are separated because this way we can occlude them easily.
	// Due to these defining cube side triangles, normals are known already.
	FixedArray<std::vector<Vector3>, Cube::SIDE_COUNT> _model_side_positions;
	FixedArray<std::vector<Vector2>, Cube::SIDE_COUNT> _model_side_uvs;
	FixedArray<std::vector<int>, Cube::SIDE_COUNT> _model_side_indices;

	// TODO Child voxel types?
};

VARIANT_ENUM_CAST(Voxel::GeometryType)
VARIANT_ENUM_CAST(Voxel::Side)

#endif // VOXEL_TYPE_H
