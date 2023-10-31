#ifndef VOXEL_BLOCKY_LIBRARY_BASE_H
#define VOXEL_BLOCKY_LIBRARY_BASE_H

#include "../../util/containers/dynamic_bitset.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/thread/rw_lock.h"
#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Base class for libraries that can be used with VoxelMesherBlocky.
// A library provides a set of pre-processed models that can be efficiently batched into a voxel mesh.
// Depending on the type of library, these models are provided differently.
class VoxelBlockyLibraryBase : public Resource {
	GDCLASS(VoxelBlockyLibraryBase, Resource)

public:
	// Limit based on maximum supported by VoxelMesherBlocky
	static const unsigned int MAX_MODELS = 65536;
	static const uint32_t NULL_INDEX = 0xFFFFFFFF;

	struct BakedData {
		// 2D array: { X : pattern A, Y : pattern B } => Does A occlude B
		// Where index is X + Y * pattern count
		DynamicBitset side_pattern_culling;
		unsigned int side_pattern_count = 0;
		// Lots of data can get moved but it's only on load.
		std::vector<VoxelBlockyModel::BakedData> models;

		// struct VariantInfo {
		// 	uint16_t type_index;
		// 	FixedArray<uint8_t, 4> attributes;
		// };

		// std::vector<VariantInfo> variant_infos;

		unsigned int indexed_materials_count = 0;

		inline bool has_model(uint32_t i) const {
			return i < models.size();
		}

		inline bool get_side_pattern_occlusion(unsigned int pattern_a, unsigned int pattern_b) const {
#ifdef DEBUG_ENABLED
			CRASH_COND(pattern_a >= side_pattern_count);
			CRASH_COND(pattern_b >= side_pattern_count);
#endif
			return side_pattern_culling.get(pattern_a + pattern_b * side_pattern_count);
		}
	};

	bool get_bake_tangents() const {
		return _bake_tangents;
	}
	void set_bake_tangents(bool bt);

	//

	virtual void load_default();
	virtual void clear();

	virtual void bake();

	//-------------------------
	// Internal use

	const BakedData &get_baked_data() const {
		return _baked_data;
	}
	const RWLock &get_baked_data_rw_lock() const {
		return _baked_data_rw_lock;
	}

	Ref<Material> get_material_by_index(unsigned int index) const;

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const;
#endif

private:
	// Convenience method to get all indexed materials after baking,
	// which can be passed to VoxelMesher::build for testing
	TypedArray<Material> _b_get_materials() const;
	void _b_bake();

	static void _bind_methods();

protected:
	int _legacy_atlas_size = 16;
	bool _needs_baking = true;
	bool _bake_tangents = true;

	// Used in multithread context by the mesher. Don't modify that outside of bake().
	RWLock _baked_data_rw_lock;
	BakedData _baked_data;
	// One of the entries can be null to represent "The default material". If all non-empty models have materials, there
	// won't be a null entry.
	std::vector<Ref<Material>> _indexed_materials;
};

void generate_side_culling_matrix(VoxelBlockyLibraryBase::BakedData &baked_data);

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_LIBRARY_BASE_H
