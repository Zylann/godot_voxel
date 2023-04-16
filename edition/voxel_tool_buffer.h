#ifndef VOXEL_TOOL_BUFFER_H
#define VOXEL_TOOL_BUFFER_H

#include "voxel_tool.h"

namespace zylann::voxel {

class VoxelToolBuffer : public VoxelTool {
	GDCLASS(VoxelToolBuffer, VoxelTool)
public:
	VoxelToolBuffer() {}
	VoxelToolBuffer(Ref<gd::VoxelBuffer> vb);

	bool is_area_editable(const Box3i &box) const override;
	void paste(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask) override;
	void paste_masked(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel,
			uint64_t mask_value) override;

	void set_voxel_metadata(Vector3i pos, Variant meta) override;
	Variant get_voxel_metadata(Vector3i pos) const override;

	void do_sphere(Vector3 center, float radius) override;

protected:
	uint64_t _get_voxel(Vector3i pos) const override;
	float _get_voxel_f(Vector3i pos) const override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Box3i &box) override;

private:
	Ref<gd::VoxelBuffer> _buffer;
};

} // namespace zylann::voxel

#endif // VOXEL_TOOL_BUFFER_H
