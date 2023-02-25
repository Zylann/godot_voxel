#ifndef TRANSVOXEL_H
#define TRANSVOXEL_H

#include "../../storage/voxel_buffer_internal.h"
#include "../../util/fixed_array.h"
#include "../../util/math/color.h"
#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"

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
// Transvoxel guarantees a maximum number of triangle generated for each 2x2x2 cell of voxels.
static const unsigned int MAX_TRIANGLES_PER_CELL = 5;

enum TexturingMode {
	TEXTURES_NONE,
	// Blends the 4 most-represented textures in the given block, ignoring the others.
	// Texture indices and blend factors have 4-bit precision (maximum 16 textures and 16 transition gradients),
	// and are respectively encoded in UV.x and UV.y.
	TEXTURES_BLEND_4_OVER_16
};

struct LodAttrib {
	Vector3f secondary_position;
	// Mask telling if a cell the vertex belongs to is on a side of the block.
	// Each bit corresponds to a side.
	// 0: -X
	// 1: +X
	// 2: -Y
	// 3: +Y
	// 4: -Z
	// 5: +Z
	uint8_t cell_border_mask;
	// Mask telling if the vertex is on a side of the block. Same convention as above.
	uint8_t vertex_border_mask;
	// Flag telling if the vertex belongs to a transition mesh.
	uint8_t transition;
	// Unused. Necessary to align to 4*sizeof(float) so it can be memcpied to a rendering buffer.
	uint8_t _pad;
};

// struct TextureAttrib {
// 	uint8_t index0;
// 	uint8_t index1;
// 	uint8_t index2;
// 	uint8_t index3;
// 	uint8_t weight0;
// 	uint8_t weight1;
// 	uint8_t weight2;
// 	uint8_t weight3;
// };

struct MeshArrays {
	std::vector<Vector3f> vertices;
	std::vector<Vector3f> normals;
	std::vector<LodAttrib> lod_data;
	std::vector<Vector2f> texturing_data; // TextureAttrib
	std::vector<int32_t> indices;

	void clear() {
		vertices.clear();
		normals.clear();
		lod_data.clear();
		texturing_data.clear();
		indices.clear();
	}

	int add_vertex(Vector3f primary, Vector3f normal, uint8_t cell_border_mask, uint8_t vertex_border_mask,
			uint8_t transition, Vector3f secondary) {
		int vi = vertices.size();
		vertices.push_back(primary);
		normals.push_back(normal);
		lod_data.push_back({ secondary, cell_border_mask, vertex_border_mask, transition, 0 });
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
	virtual ~IDeepSDFSampler() {}
	virtual float get_single(const Vector3i position_in_voxels, uint32_t lod_index) const = 0;
};

struct CellInfo {
	Vector3i position;
	uint32_t triangle_count;
};

DefaultTextureIndicesData build_regular_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		const IDeepSDFSampler *deep_sdf_sampler, std::vector<CellInfo> *cell_infos);

void build_transition_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel, int direction,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		DefaultTextureIndicesData default_texture_indices_data);

} // namespace zylann::voxel::transvoxel

#endif // TRANSVOXEL_H
