#ifndef VOXEL_BLOCKY_FLUID_H
#define VOXEL_BLOCKY_FLUID_H

#include "../../util/godot/classes/material.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/math/rect2i.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Minecraft-style fluid common configuration.
// Fluids are a bit special compared to regular models. Rendering them with precalculated models would require way too
// many of them. So instead, they are procedurally generated during meshing.
class VoxelBlockyFluid : public Resource {
	GDCLASS(VoxelBlockyFluid, Resource)
public:
	struct BakedData {
		static constexpr float TOP_HEIGHT = 0.9375f;
		static constexpr float BOTTOM_HEIGHT = 0.0625f;

		FixedArray<VoxelBlockyModel::BakedData::SideSurface, Cube::SIDE_COUNT> side_surfaces;
		StdVector<uint16_t> level_model_indices;
		uint32_t material_id = 0;
		// uint32_t box_collision_mask = 0;
	};

	VoxelBlockyFluid();

	void set_flowing_tile_strip(Rect2i tile_rect);
	Rect2i get_flowing_tile_strip() const;

	void set_idle_tile_strip(Rect2i tile_rect);
	Rect2i get_idle_tile_strip() const;

	void set_material(Ref<Material> material);
	Ref<Material> get_material() const;

private:
	static void _bind_methods();

	Rect2i _flowing_tile_strip = Rect2i(0, 0, 1, 1);
	Rect2i _idle_tile_strip = Rect2i(0, 0, 1, 1);
	Vector2i _atlas_size_in_tiles = Vector2i(16, 16);
	Ref<Material> _material;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_FLUID_H