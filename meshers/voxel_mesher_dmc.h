#ifndef VOXEL_MESHER_DMC_H
#define VOXEL_MESHER_DMC_H

#include "../util/thread/mutex.h"
#include "voxel_mesher.h"

namespace zylann::voxel {

class VoxelMesherDMC : public VoxelMesher {
	GDCLASS(VoxelMesherDMC, VoxelMesher)
public:
	enum MaterialMode { //
		MATERIAL_MODE_NONE = 0,
		// Interpret voxels as having only one 8-bit material index, and output vertex data containing 2 8-bit indices
		// and 1 blend value (similar to No Man's Sky)
		// TODO I don't know what kind of better name this should have
		MATERIAL_MODE_V_1I8_S_2I8_1W8,
		MATERIAL_MODE_COUNT
	};

	VoxelMesherDMC();
	~VoxelMesherDMC();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	int get_used_channels_mask() const override;

	bool supports_lod() const override {
		return true;
	}

	bool is_skirts_enabled() const;
	void set_skirts_enabled(bool enabled);

	MaterialMode get_material_mode() const;
	void set_material_mode(MaterialMode mode);

private:
	void update_padding();

	static void _bind_methods();

	bool _skirts_enabled = true;
	MaterialMode _material_mode = MATERIAL_MODE_NONE;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherDMC::MaterialMode);

#endif // VOXEL_MESHER_DMC_H
