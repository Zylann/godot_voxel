#ifndef VOXEL_BLOCKY_MODEL_FLUID_H
#define VOXEL_BLOCKY_MODEL_FLUID_H

#include "voxel_blocky_fluid.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Minecraft-style fluid model for a specific level.
class VoxelBlockyModelFluid : public VoxelBlockyModel {
	GDCLASS(VoxelBlockyModelFluid, VoxelBlockyModel)
public:
	static const int MAX_LEVELS = 256;

	VoxelBlockyModelFluid();

	void set_fluid(Ref<VoxelBlockyFluid> fluid);
	Ref<VoxelBlockyFluid> get_fluid() const;

	void set_level(int level);
	int get_level() const;

	void bake(blocky::ModelBakingContext &ctx) const override;

	// #ifdef TOOLS_ENABLED
	// 	void get_configuration_warnings(PackedStringArray &warnings);
	// #endif

private:
	static void _bind_methods();

	Ref<VoxelBlockyFluid> _fluid;
	unsigned int _level = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_FLUID_H
