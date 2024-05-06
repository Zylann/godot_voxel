#ifndef VOXEL_BLOCKY_FLUID_H
#define VOXEL_BLOCKY_FLUID_H

#include "../../constants/cube_tables.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/math/vector3f.h"
#include <cstdint>

ZN_GODOT_FORWARD_DECLARE(class Material);

namespace zylann::voxel {

namespace blocky {
struct MaterialIndexer;
}

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

	struct Surface {
		StdVector<Vector3f> positions;
		StdVector<int> indices;
		StdVector<float> tangents;
		// Normals aren't stored because they are assumed to be the same for the whole side

		void clear() {
			positions.clear();
			indices.clear();
			tangents.clear();
		}
	};

	struct BakedData {
		static constexpr float TOP_HEIGHT = 0.9375f;
		static constexpr float BOTTOM_HEIGHT = 0.0625f;

		FixedArray<Surface, Cube::SIDE_COUNT> side_surfaces;

		// StdVector<uint16_t> level_model_indices;
		uint32_t material_id = 0;
		uint8_t max_level = 1;
		bool dip_when_flowing_down = false;
		// uint32_t box_collision_mask = 0;
	};

	VoxelBlockyFluid();

	void set_material(Ref<Material> material);
	Ref<Material> get_material() const;

	void set_dip_when_flowing_down(bool enable);
	bool get_dip_when_flowing_down() const;

	void bake(BakedData &baked_fluid, blocky::MaterialIndexer &materials) const;

private:
	static void _bind_methods();

	Ref<Material> _material;
	bool _dip_when_flowing_down = false;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_FLUID_H
