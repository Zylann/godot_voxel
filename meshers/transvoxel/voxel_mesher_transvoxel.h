#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

//#include "../../constants/cube_tables.h"
#include "../../util/fixed_array.h"
#include "../voxel_mesher.h"
#include <vector>

namespace Transvoxel {

// How many extra voxels are needed towards the negative axes
static const int MIN_PADDING = 1;
// How many extra voxels are needed towards the positive axes
static const int MAX_PADDING = 2;
// How many textures can be referred to in total
static const unsigned int MAX_TEXTURES = 16;
// How many textures can blend at once
static const unsigned int MAX_TEXTURE_BLENDS = 4;

enum TexturingMode {
	TEXTURES_NONE,
	// Blends the 4 most-represented textures in the given block, ignoring the others.
	// Texture indices and blend factors have 4-bit precision (maximum 16 textures),
	// and are respectively encoded in UV and UV2.
	TEXTURES_BLEND_4_OVER_16
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

	int add_vertex(Vector3 primary, Vector3 normal, uint16_t border_mask, Vector3 secondary) {
		int vi = vertices.size();
		vertices.push_back(primary);
		normals.push_back(normal);
		extra.push_back(Color(secondary.x, secondary.y, secondary.z, border_mask));
		return vi;
	}
};

struct ReuseCell {
	FixedArray<int, 4> vertices;
	unsigned int packed_texture_indices;
};

struct ReuseTransitionCell {
	FixedArray<int, 12> vertices;
};

class Cache {
public:
	void reset_reuse_cells(Vector3i p_block_size) {
		_block_size = p_block_size;
		unsigned int deck_area = _block_size.x * _block_size.y;
		for (unsigned int i = 0; i < _cache.size(); ++i) {
			std::vector<ReuseCell> &deck = _cache[i];
			deck.resize(deck_area);
			for (size_t j = 0; j < deck.size(); ++j) {
				deck[j].vertices.fill(-1);
			}
		}
	}

	void reset_reuse_cells_2d(Vector3i p_block_size) {
		for (unsigned int i = 0; i < _cache_2d.size(); ++i) {
			std::vector<ReuseTransitionCell> &row = _cache_2d[i];
			row.resize(p_block_size.x);
			for (size_t j = 0; j < row.size(); ++j) {
				row[j].vertices.fill(-1);
			}
		}
	}

	ReuseCell &get_reuse_cell(Vector3i pos) {
		unsigned int j = pos.z & 1;
		unsigned int i = pos.y * _block_size.y + pos.x;
		CRASH_COND(i >= _cache[j].size());
		return _cache[j][i];
	}

	ReuseTransitionCell &get_reuse_cell_2d(int x, int y) {
		unsigned int j = y & 1;
		unsigned int i = x;
		CRASH_COND(i >= _cache_2d[j].size());
		return _cache_2d[j][i];
	}

private:
	FixedArray<std::vector<ReuseCell>, 2> _cache;
	FixedArray<std::vector<ReuseTransitionCell>, 2> _cache_2d;
	Vector3i _block_size;
};

} // namespace Transvoxel

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
