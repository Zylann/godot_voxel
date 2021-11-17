#ifndef TRANSVOXEL_H
#define TRANSVOXEL_H

#include "../../storage/voxel_buffer_internal.h"
#include "../../util/fixed_array.h"
#include "../../util/math/vector3i.h"

#include <core/color.h>
#include <core/math/vector2.h>
#include <core/math/vector3.h>
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
	// and are respectively encoded in UV.x and UV.y.
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
	unsigned int packed_texture_indices = 0;
};

struct ReuseTransitionCell {
	FixedArray<int, 12> vertices;
	unsigned int packed_texture_indices = 0;
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
		unsigned int i = pos.y * _block_size.x + pos.x;
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

// This is only to re-use some data computed for regular mesh into transition meshes
struct DefaultTextureIndicesData {
	FixedArray<uint8_t, 4> indices;
	uint32_t packed_indices;
	bool use;
};

DefaultTextureIndicesData build_regular_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel, int lod_index,
		TexturingMode texturing_mode, Cache &cache, MeshArrays &output);

void build_transition_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel, int direction, int lod_index,
		TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		DefaultTextureIndicesData default_texture_indices_data);

} // namespace Transvoxel

#endif // TRANSVOXEL_H
