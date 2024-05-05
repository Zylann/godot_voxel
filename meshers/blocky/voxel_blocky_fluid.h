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
	enum FlowState : uint8_t {
		// o---x
		// |
		// z
		// Values are proportional to an angle, and named after a top-down OpenGL coordinate system.
		FLOW_STRAIGHT_POSITIVE_X,
		FLOW_DIAGONAL_POSITIVE_X_NEGATIVE_Z,
		FLOW_STRAIGHT_NEGATIVE_Z,
		FLOW_DIAGONAL_NEGATIVE_X_NEGATIVE_Z,
		FLOW_STRAIGHT_NEGATIVE_X,
		FLOW_DIAGONAL_NEGATIVE_X_POSITIVE_Z,
		FLOW_STRAIGHT_POSITIVE_Z,
		FLOW_DIAGONAL_POSITIVE_X_POSITIVE_Z,
		FLOW_IDLE,
		FLOW_STATE_COUNT
	};

	struct BakedData {
		static constexpr float TOP_HEIGHT = 0.9375f;
		static constexpr float BOTTOM_HEIGHT = 0.0625f;

		FixedArray<VoxelBlockyModel::BakedData::SideSurface, Cube::SIDE_COUNT> side_surfaces;

		// StdVector<uint16_t> level_model_indices;
		uint32_t material_id = 0;
		uint8_t max_level = 1;
		// uint32_t box_collision_mask = 0;
	};

	VoxelBlockyFluid();

	void set_material(Ref<Material> material);
	Ref<Material> get_material() const;

	void bake(BakedData &baked_fluid, VoxelBlockyModel::MaterialIndexer &materials) const;

private:
	static void _bind_methods();

	Ref<Material> _material;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_FLUID_H
