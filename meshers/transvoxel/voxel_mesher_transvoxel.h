#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../voxel_mesher.h"
#include "transvoxel.h"

class ArrayMesh;

class VoxelMesherTransvoxel : public VoxelMesher {
	GDCLASS(VoxelMesherTransvoxel, VoxelMesher)

public:
	enum TexturingMode {
		TEXTURES_NONE = Transvoxel::TEXTURES_NONE,
		TEXTURES_BLEND_4_OVER_16 = Transvoxel::TEXTURES_BLEND_4_OVER_16
	};

	VoxelMesherTransvoxel();
	~VoxelMesherTransvoxel();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;
	Ref<ArrayMesh> build_transition_mesh(Ref<VoxelBuffer> voxels, int direction);

	Ref<Resource> duplicate(bool p_subresources = false) const override;
	int get_used_channels_mask() const override;

	void set_texturing_mode(TexturingMode mode);
	TexturingMode get_texturing_mode() const;

protected:
	static void _bind_methods();

private:
	void fill_surface_arrays(Array &arrays, const Transvoxel::MeshArrays &src);

	TexturingMode _texture_mode = TEXTURES_NONE;
};

VARIANT_ENUM_CAST(VoxelMesherTransvoxel::TexturingMode);

#endif // VOXEL_MESHER_TRANSVOXEL_H
