#ifndef TRANSVOXEL_H
#define TRANSVOXEL_H

#include "../../storage/voxel_buffer_internal.h"
#include "../../util/fixed_array.h"
#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"

#include <core/math/color.h>
#include <vector>

namespace zylann::voxel::transvoxel {

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
	// Texture indices and blend factors have 4-bit precision (maximum 16 textures and 16 transition gradients),
	// and are respectively encoded in UV.x and UV.y.
	TEXTURES_BLEND_4_OVER_16
};

struct MeshArrays {
	std::vector<Vector3f> vertices;
	std::vector<Vector3f> normals;
	std::vector<Color> lod_data;
	std::vector<Vector2f> texturing_data;
	std::vector<int> indices;

	void clear() {
		vertices.clear();
		normals.clear();
		lod_data.clear();
		texturing_data.clear();
		indices.clear();
	}

	int add_vertex(Vector3f primary, Vector3f normal, uint16_t border_mask, Vector3f secondary) {
		int vi = vertices.size();
		vertices.push_back(primary);
		normals.push_back(normal);
		lod_data.push_back(Color(secondary.x, secondary.y, secondary.z, border_mask));
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
		const unsigned int deck_area = _block_size.x * _block_size.y;
		for (unsigned int i = 0; i < _cache.size(); ++i) {
			std::vector<ReuseCell> &deck = _cache[i];
			deck.resize(deck_area);
			for (size_t j = 0; j < deck.size(); ++j) {
				fill(deck[j].vertices, -1);
			}
		}
	}

	void reset_reuse_cells_2d(Vector3i p_block_size) {
		for (unsigned int i = 0; i < _cache_2d.size(); ++i) {
			std::vector<ReuseTransitionCell> &row = _cache_2d[i];
			row.resize(p_block_size.x);
			for (size_t j = 0; j < row.size(); ++j) {
				fill(row[j].vertices, -1);
			}
		}
	}

	ReuseCell &get_reuse_cell(Vector3i pos) {
		unsigned int j = pos.z & 1;
		unsigned int i = pos.y * _block_size.x + pos.x;
		ZN_ASSERT(i < _cache[j].size());
		return _cache[j][i];
	}

	ReuseTransitionCell &get_reuse_cell_2d(int x, int y) {
		unsigned int j = y & 1;
		unsigned int i = x;
		ZN_ASSERT(i < _cache_2d[j].size());
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

class IDeepSDFSampler {
public:
	virtual float get_single(const Vector3i position_in_voxels, uint32_t lod_index) const = 0;
};

DefaultTextureIndicesData build_regular_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		const IDeepSDFSampler *deep_sdf_sampler);

void build_transition_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel, int direction,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		DefaultTextureIndicesData default_texture_indices_data);

} //namespace zylann::voxel::transvoxel

#endif // TRANSVOXEL_H
