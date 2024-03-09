#ifndef VOXEL_TOOL_MULTIPASS_GENERATOR_H
#define VOXEL_TOOL_MULTIPASS_GENERATOR_H

#include "../../edition/voxel_tool.h"
#include "voxel_generator_multipass_cb_structs.h"

namespace zylann::voxel {

// Accessor provided to scripts in the context of generating columns of blocks in multipass generators.
// It is not supposed to be used by more than one thread at a time.
// Scripts are not allowed to keep a reference to it outside of the method it is passed in (unfortunately Godot doesn't
// provide something to guard against it)
class VoxelToolMultipassGenerator : public VoxelTool {
	GDCLASS(VoxelToolMultipassGenerator, VoxelTool)
public:
	void set_pass_input(VoxelGeneratorMultipassCBStructs::PassInput &pass_input);

	// VoxelTool methods

	void copy(Vector3i pos, VoxelBuffer &dst, uint8_t channels_mask) const override;
	void paste(Vector3i pos, const VoxelBuffer &src, uint8_t channels_mask) override;
	void paste_masked(Vector3i pos, Ref<godot::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel,
			uint64_t mask_value) override;

	bool is_area_editable(const Box3i &box) const override;

	void do_path(Span<const Vector3> positions, Span<const float> radii) override;

	// TODO Implement more methods

	// Specific methods

	Vector3i get_editable_area_min() const;
	Vector3i get_editable_area_max() const;

	Vector3i get_main_area_min() const;
	Vector3i get_main_area_max() const;

	// Debug

	// Create a standalone instance for testing purposes.
	// static Ref<VoxelToolMultipassGenerator> create_offline(
	// 		Vector3i grid_origin_blocks, Vector3i grid_size_blocks, Vector3i main_block_position, int block_size_po2);

protected:
	uint64_t _get_voxel(Vector3i pos) const override;
	float _get_voxel_f(Vector3i pos) const override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Box3i &box) override;

	static void _bind_methods();

private:
	VoxelGeneratorMultipassCBStructs::Block *get_block_and_relative_position(
			Vector3i terrain_voxel_pos, Vector3i &out_voxel_rpos) const;

	VoxelGeneratorMultipassCBStructs::PassInput _pass_input;
	int _block_size_po2 = 0;
	int _block_size_mask = 0;
	Box3i _editable_voxel_box;

	// "offline" means the class uses its own storage for testing purposes.
	// bool _is_offline = false;
	// StdVector<VoxelGeneratorMultipassCBStructs::Block> _offline_blocks;
	// StdVector<VoxelGeneratorMultipassCBStructs::Block *> _offline_block_pointers;
};

} // namespace zylann::voxel

#endif // VOXEL_TOOL_MULTIPASS_GENERATOR_H
