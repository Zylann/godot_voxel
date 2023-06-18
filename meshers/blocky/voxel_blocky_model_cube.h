#ifndef VOXEL_BLOCKY_MODEL_CUBE_H
#define VOXEL_BLOCKY_MODEL_CUBE_H

#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Cubic model, with configurable tiles on each side
// TODO Would it be better to add a new PrimitiveMesh doing this, and use VoxelBlockyMesh?
class VoxelBlockyModelCube : public VoxelBlockyModel {
	GDCLASS(VoxelBlockyModelCube, VoxelBlockyModel)
public:
	VoxelBlockyModelCube();

	static Cube::Side name_to_side(const String &s);

	Vector2i get_tile(VoxelBlockyModel::Side side) const {
		return _tiles[side];
	}

	void set_tile(VoxelBlockyModel::Side side, Vector2i pos);

	void set_height(float h);
	float get_height() const;

	void set_atlas_size_in_tiles(Vector2i s);
	Vector2i get_atlas_size_in_tiles() const;

	void bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const override;
	bool is_empty() const override;

	Ref<Mesh> get_preview_mesh() const override;

	void rotate_90(math::Axis axis, bool clockwise) override;
	void rotate_ortho(math::OrthoBasis ortho_basis) override;

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	FixedArray<Vector2i, Cube::SIDE_COUNT> _tiles;
	float _height = 1.f;

	Vector2i _atlas_size_in_tiles;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_CUBE_H
