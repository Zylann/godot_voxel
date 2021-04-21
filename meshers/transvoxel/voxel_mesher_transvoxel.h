#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../../constants/cube_tables.h"
#include "../../util/fixed_array.h"
#include "../voxel_mesher.h"
#include <vector>

class VoxelMesherTransvoxelInternal {
public:
	static const int MIN_PADDING = 1;
	static const int MAX_PADDING = 2;
	static const unsigned int MAX_TEXTURES = 16;
	static const unsigned int MAX_TEXTURE_BLENDS = 4;

	enum TexturingMode {
		TEXTURES_NONE,
		//TEXTURES_BLEND_4,
		// Blends the 4 most-represented textures in the given block, ignoring the others.
		// Texture indices and blend factors have 4-bit precision (maximum 16 textures),
		// and are respectively encoded in UV and UV2.
		TEXTURES_BLEND_4_OVER_16
	};

	struct TexturingData {
		FixedArray<uint8_t, MAX_TEXTURE_BLENDS> indices;
		FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights;
	};

	struct MeshArrays {
		std::vector<Vector3> vertices;
		std::vector<Vector3> normals;
		std::vector<Color> extra;
		std::vector<Vector2> uv;
		std::vector<int> indices;

		void clear() {
			vertices.clear();
			normals.clear();
			extra.clear();
			uv.clear();
			indices.clear();
		}
	};

	void build_internal(const VoxelBuffer &voxels, unsigned int sdf_channel, int lod_index,
			TexturingMode texturing_mode, FixedArray<uint8_t, 4> &out_texture_indices);
	void build_transition(const VoxelBuffer &voxels, unsigned int sdf_channel, int direction, int lod_index,
			TexturingMode texturing_mode, const FixedArray<uint8_t, 4> &texture_indices);
	void clear_output() { _output.clear(); }
	const MeshArrays &get_output() const { return _output; }

private:
	struct ReuseCell {
		FixedArray<int, 4> vertices;
		unsigned int packed_texture_indices;
	};

	struct ReuseTransitionCell {
		FixedArray<int, 12> vertices;
	};

	struct TransitionVoxels {
		const VoxelBuffer *full_resolution_neighbor_voxels[Cube::SIDE_COUNT] = { nullptr };
	};

	void reset_reuse_cells(Vector3i block_size);
	void reset_reuse_cells_2d(Vector3i block_size);
	ReuseCell &get_reuse_cell(Vector3i pos);
	ReuseTransitionCell &get_reuse_cell_2d(int x, int y);
	int emit_vertex(Vector3 primary, Vector3 normal, uint16_t border_mask, Vector3 secondary);

private:
	// Work cache
	FixedArray<std::vector<ReuseCell>, 2> _cache;
	FixedArray<std::vector<ReuseTransitionCell>, 2> _cache_2d;
	Vector3i _block_size;
	MeshArrays _output;
};

class ArrayMesh;

class VoxelMesherTransvoxel : public VoxelMesher {
	GDCLASS(VoxelMesherTransvoxel, VoxelMesher)

public:
	enum TexturingMode {
		TEXTURES_NONE = VoxelMesherTransvoxelInternal::TEXTURES_NONE,
		TEXTURES_BLEND_4_OVER_16 = VoxelMesherTransvoxelInternal::TEXTURES_BLEND_4_OVER_16
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
	void fill_surface_arrays(Array &arrays, const VoxelMesherTransvoxelInternal::MeshArrays &src);

	static thread_local VoxelMesherTransvoxelInternal _impl;

	TexturingMode _texture_mode = TEXTURES_NONE;
};

VARIANT_ENUM_CAST(VoxelMesherTransvoxel::TexturingMode);

#endif // VOXEL_MESHER_TRANSVOXEL_H
