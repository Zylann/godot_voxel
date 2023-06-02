#ifndef VOXEL_BLOCKY_MODEL_H
#define VOXEL_BLOCKY_MODEL_H

#include "../../constants/cube_tables.h"
#include "../../util/fixed_array.h"
#include "../../util/godot/classes/material.h"
#include "../../util/godot/classes/mesh.h"
#include "../../util/macros.h"
#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"

#include <vector>

namespace zylann::voxel {

// Definition of one type of voxel for use with `VoxelMesherBlocky`.
// A voxel can be a simple coloured cube, or a more complex model.
// Important: it is recommended that you create voxels from a library rather than using new().
class VoxelBlockyModel : public Resource {
	GDCLASS(VoxelBlockyModel, Resource)

public:
	// Convention to mean "nothing".
	// Don't assign a non-empty model at this index.
	static const uint16_t AIR_ID = 0;

	// Plain data strictly used by the mesher.
	// It becomes distinct because it's going to be used in a multithread environment,
	// while the configuration that produced the data can be changed by the user at any time.
	struct BakedData {
		struct Surface {
			// Inside part of the model.
			std::vector<Vector3f> positions;
			std::vector<Vector3f> normals;
			std::vector<Vector2f> uvs;
			std::vector<int> indices;
			std::vector<float> tangents;
			// Model sides:
			// They are separated because this way we can occlude them easily.
			// Due to these defining cube side triangles, normals are known already.
			FixedArray<std::vector<Vector3f>, Cube::SIDE_COUNT> side_positions;
			FixedArray<std::vector<Vector2f>, Cube::SIDE_COUNT> side_uvs;
			FixedArray<std::vector<int>, Cube::SIDE_COUNT> side_indices;
			FixedArray<std::vector<float>, Cube::SIDE_COUNT> side_tangents;

			uint32_t material_id = 0;
			bool collision_enabled = true;

			void clear() {
				positions.clear();
				normals.clear();
				uvs.clear();
				indices.clear();
				tangents.clear();

				for (int side = 0; side < Cube::SIDE_COUNT; ++side) {
					side_positions[side].clear();
					side_uvs[side].clear();
					side_indices[side].clear();
					side_tangents[side].clear();
				}
			}
		};

		struct Model {
			static constexpr uint32_t MAX_SURFACES = 2;

			// A model can have up to 2 materials.
			// If more is needed or profiling tells better, we could change it to a vector?
			FixedArray<Surface, MAX_SURFACES> surfaces;
			unsigned int surface_count = 0;
			// Cached information to check this case early
			uint8_t empty_sides_mask = 0;

			// Tells what is the "shape" of each side in order to cull them quickly when in contact with neighbors.
			// Side patterns are still determined based on a combination of all surfaces.
			FixedArray<uint32_t, Cube::SIDE_COUNT> side_pattern_indices;

			void clear() {
				for (unsigned int i = 0; i < surfaces.size(); ++i) {
					surfaces[i].clear();
				}
			}
		};

		Model model;
		Color color;
		uint8_t transparency_index;
		bool contributes_to_ao;
		bool empty;

		inline void clear() {
			model.clear();
			empty = true;
		}
	};

	VoxelBlockyModel();

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

	void set_voxel_name(String p_name);
	_FORCE_INLINE_ StringName get_voxel_name() const {
		return _name;
	}

	void set_id(int id);
	_FORCE_INLINE_ int get_id() const {
		return _id;
	}

	void set_color(Color color);
	_FORCE_INLINE_ Color get_color() const {
		return _color;
	}

	void set_material_override(int index, Ref<Material> material);
	Ref<Material> get_material_override(int index) const;

	void set_mesh_collision_enabled(int surface_index, bool enabled);
	bool is_mesh_collision_enabled(int surface_index) const;

	// TODO Might become obsoleted by transparency index
	void set_transparent(bool t = true);
	_FORCE_INLINE_ bool is_transparent() const {
		return _transparency_index != 0;
	}

	void set_transparency_index(int i);
	int get_transparency_index() const {
		return _transparency_index;
	}

	void set_custom_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_custom_mesh() const {
		return _custom_mesh;
	}

	void set_random_tickable(bool rt);
	inline bool is_random_tickable() const {
		return _random_tickable;
	}

	void set_collision_mask(uint32_t mask);
	inline uint32_t get_collision_mask() const {
		return _collision_mask;
	}

	Vector2f get_cube_tile(int side) const {
		return _cube_tiles[side];
	}

	//-------------------------------------------
	// Built-in geometry generators

	enum GeometryType { //
		GEOMETRY_NONE = 0,
		GEOMETRY_CUBE,
		GEOMETRY_CUSTOM_MESH,
		GEOMETRY_MAX
	};

	void set_geometry_type(GeometryType type);
	GeometryType get_geometry_type() const;

	inline bool is_empty() const {
		return _empty;
	}

	Ref<Resource> duplicate(bool p_subresources) const ZN_OVERRIDE_UNLESS_GODOT_EXTENSION;

	//------------------------------------------
	// Properties for internal usage only

	struct MaterialIndexer {
		std::vector<Ref<Material>> &materials;

		unsigned int get_or_create_index(const Ref<Material> &p_material) {
			for (size_t i = 0; i < materials.size(); ++i) {
				const Ref<Material> &material = materials[i];
				if (material == p_material) {
					return i;
				}
			}
			const unsigned int ret = materials.size();
			materials.push_back(p_material);
			return ret;
		}
	};

	void bake(BakedData &baked_data, int p_atlas_size, bool bake_tangents, MaterialIndexer &materials);

	const std::vector<AABB> &get_collision_aabbs() const {
		return _collision_aabbs;
	}

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void set_cube_uv_side(int side, Vector2f tile_pos);

	static void _bind_methods();

	TypedArray<AABB> _b_get_collision_aabbs() const;
	void _b_set_collision_aabbs(TypedArray<AABB> array);

private:
	void set_surface_count(unsigned int new_count);

	// Identifiers

	int _id = -1;
	StringName _name;

	// Properties

	struct SurfaceParams {
		// If assigned, these materials override those present on the mesh itself.
		Ref<Material> material_override;
		// If true and classic mesh physics are enabled, the surface will be present in the collider.
		bool collision_enabled = true;
	};

	FixedArray<SurfaceParams, BakedData::Model::MAX_SURFACES> _surface_params;
	unsigned int _surface_count = 0;

	// If two neighboring voxels are supposed to occlude their shared face,
	// this index decides wether or not it should happen. Equal indexes culls the face, different indexes doesn't.
	uint8_t _transparency_index = 0;

	Color _color;
	GeometryType _geometry_type = GEOMETRY_NONE;
	FixedArray<Vector2f, Cube::SIDE_COUNT> _cube_tiles;
	Ref<Mesh> _custom_mesh;
	bool _random_tickable = false;
	bool _empty = true;
	// Used for AABB physics only, not classic physics
	std::vector<AABB> _collision_aabbs;
	uint32_t _collision_mask = 1;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelBlockyModel::GeometryType)
VARIANT_ENUM_CAST(zylann::voxel::VoxelBlockyModel::Side)

#endif // VOXEL_BLOCKY_MODEL_H
