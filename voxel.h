#ifndef VOXEL_TYPE_H
#define VOXEL_TYPE_H

#include <resource.h>

class VoxelLibrary;

// Definition of one type of voxel.
// A voxel can be a simple coloured cube, or a more complex model.
// Important: it is recommended that you create voxels from a library rather than using new().
class Voxel : public Resource {
	GDCLASS(Voxel, Resource)

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

	enum ChannelMode {
		// For mapping to a Voxel type
		CHANNEL_TYPE = 0,
		// Distance to surface for smooth voxels
		CHANNEL_ISOLEVEL,
		// Arbitrary data not used by the module
		CHANNEL_DATA
	};

	Voxel();

	// Properties

	Ref<Voxel> set_voxel_name(String name);
	_FORCE_INLINE_ String get_voxel_name() const { return _name; }

	Ref<Voxel> set_id(int id);
	_FORCE_INLINE_ int get_id() const { return _id; }

	Ref<Voxel> set_color(Color color);
	_FORCE_INLINE_ Color get_color() const { return _color; }

	Ref<Voxel> set_material_id(unsigned int id);
	_FORCE_INLINE_ unsigned int get_material_id() const { return _material_id; }

	Ref<Voxel> set_transparent(bool t = true);
	_FORCE_INLINE_ bool is_transparent() const { return _is_transparent; }

	//-------------------------------------------
	// Built-in geometry generators

	enum GeometryType {
		GEOMETRY_NONE = 0,
		GEOMETRY_CUBE = 1,
		GEOMETRY_MAX
	};

	void set_geometry_type(GeometryType type);
	GeometryType get_geometry_type() const;

	// Getters for native usage only

	const PoolVector<Vector3> &get_model_vertices() const { return _model_vertices; }
	const PoolVector<Vector3> &get_model_normals() const { return _model_normals; }
	const PoolVector<Vector2> &get_model_uv() const { return _model_uv; }
	const PoolVector<Vector3> &get_model_side_vertices(unsigned int side) const { return _model_side_vertices[side]; }
	const PoolVector<Vector2> &get_model_side_uv(unsigned int side) const { return _model_side_uv[side]; }

	void set_library(Ref<VoxelLibrary> lib);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void set_cube_uv_side(int side, Vector2 tile_pos);
	void update_cube_uv_sides();

	VoxelLibrary *get_library() const;

	static void _bind_methods();

	Ref<Voxel> set_cube_geometry(float sy = 1);
	//Ref<Voxel> set_xquad_geometry(Vector2 atlas_pos);

private:
	WeakRef _library;

	// Identifiers
	int _id;
	String _name;

	// Properties
	int _material_id;
	bool _is_transparent;
	Color _color;
	GeometryType _geometry_type;
	float _cube_geometry_padding_y;
	Vector2 _cube_tiles[SIDE_COUNT];

	// Model
	PoolVector<Vector3> _model_vertices;
	PoolVector<Vector3> _model_normals;
	PoolVector<Vector2> _model_uv;
	PoolVector<Vector3> _model_side_vertices[SIDE_COUNT];
	PoolVector<Vector2> _model_side_uv[SIDE_COUNT];

	// TODO Child voxel types
};

VARIANT_ENUM_CAST(Voxel::ChannelMode)
VARIANT_ENUM_CAST(Voxel::GeometryType)

#endif // VOXEL_TYPE_H
