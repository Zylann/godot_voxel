#ifndef VOXEL_MESHER_DMC_H
#define VOXEL_MESHER_DMC_H

#include "../util/thread/mutex.h"
#include "voxel_mesher.h"

namespace zylann::voxel {

class VoxelMesherDMC : public VoxelMesher {
	GDCLASS(VoxelMesherDMC, VoxelMesher)
public:
	VoxelMesherDMC();
	~VoxelMesherDMC();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	int get_used_channels_mask() const override;

	bool supports_lod() const override {
		return true;
	}

	bool is_skirts_enabled() const;
	void set_skirts_enabled(bool enabled);

private:
	void update_padding();

	static void _bind_methods();

	bool _skirts_enabled = true;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_DMC_H
