#ifndef VOXEL_TYPE_H
#define VOXEL_TYPE_H

#include "../../cube_tables.h"
#include "../../util/fixed_array.h"

#include <scene/resources/mesh.h>

class VoxelLibrary;

// TODO Rename VoxelModel?
// Definition of one type of voxel for use with `VoxelMesherBlocky`.
// A voxel can be a simple coloured cube, or a more complex model.
// Important: it is recommended that you create voxels from a library rather than using new().
class Voxel : public Resource {
	GDCLASS(Voxel, Resource)

public:
	// Plain data strictly used by the mesher.
	// It becomes distinct because it's going to be used in a multithread environment,
	// while the configuration that produced the data can be changed by the user at any time.
	struct BakedData {
		struct Model {
			std::vector<Vector3> positions;
			std::vector<Vector3> normals;
			std::vector<Vector2> uvs;
			std::vector<int> indices;
			// Model sides:
			// They are separated because this way we can occlude them easily.
			// Due to these defining cube side triangles, normals are known already.
			FixedArray<std::vector<Vector3>, Cube::SIDE_COUNT> side_positions;
			FixedArray<std::vector<Vector2>, Cube::SIDE_COUNT> side_uvs;
			FixedArray<std::vector<int>, Cube::SIDE_COUNT> side_indices;

			FixedArray<uint32_t, Cube::SIDE_COUNT> side_pattern_indices;

			void clear() {
				positions.clear();
				normals.clear();
				uvs.clear();
				indices.clear();

				for (int side = 0; side < Cube::SIDE_COUNT; ++side) {
					side_positions[side].clear();
					side_uvs[side].clear();
					side_indices[side].clear();
				}
			}
		};

		Model model;
		int material_id;
		Color color;
		bool is_transparent;
		bool contributes_to_ao;
		bool empty;

		inline void clear() {
			model.clear();
			empty = true;
		}
	};

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

	void set_voxel_name(String name);
	_FORCE_INLINE_ StringName get_voxel_name() const { return _name; }

	void set_id(int id);
	_FORCE_INLINE_ int get_id() const { return _id; }

	void set_color(Color color);
	_FORCE_INLINE_ Color get_color() const { return _color; }

	void set_material_id(unsigned int id);
	_FORCE_INLINE_ unsigned int get_material_id() const { return _material_id; }

	void set_transparent(bool t = true);
	_FORCE_INLINE_ bool is_transparent() const { return _is_transparent; }

	void set_custom_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_custom_mesh() const { return _custom_mesh; }

	void set_random_tickable(bool rt);
	inline bool is_random_tickable() const { return _random_tickable; }

	void set_collision_mask(uint32_t mask);
	inline uint32_t get_collision_mask() const { return _collision_mask; }

	Vector2 get_cube_tile(int side) const { return _cube_tiles[side]; }

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

	inline bool is_empty() const { return _empty; }

	Ref<Resource> duplicate(bool p_subresources) const override;

	//------------------------------------------
	// Properties for native usage only

	void bake(BakedData &baked_data, int p_atlas_size);

	const std::vector<AABB> &get_collision_aabbs() const { return _collision_aabbs; }

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void set_cube_uv_side(int side, Vector2 tile_pos);

	static void _bind_methods();

	void set_cube_geometry();

	Array _b_get_collision_aabbs() const;
	void _b_set_collision_aabbs(Array array);

private:
	// Identifiers
	int _id;
	StringName _name;

	// Properties
	int _material_id;
	bool _is_transparent;
	Color _color;
	GeometryType _geometry_type;
	FixedArray<Vector2, Cube::SIDE_COUNT> _cube_tiles;
	Ref<Mesh> _custom_mesh;
	std::vector<AABB> _collision_aabbs;
	bool _random_tickable = false;
	bool _empty = true;
	uint32_t _collision_mask = 1;
};

VARIANT_ENUM_CAST(Voxel::GeometryType)
VARIANT_ENUM_CAST(Voxel::Side)

#endif // VOXEL_TYPE_H
