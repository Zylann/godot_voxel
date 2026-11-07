#ifndef VOXEL_BLOCKY_LIBRARY_BASE_H
#define VOXEL_BLOCKY_LIBRARY_BASE_H

#include "../../util/containers/dynamic_bitset.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/material.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/thread/rw_lock.h"
#include "blocky_baked_library.h"

namespace zylann::voxel {

// Base class for libraries that can be used with VoxelMesherBlocky.
// A library provides a set of pre-processed models that can be efficiently batched into a voxel mesh.
// Depending on the type of library, these models are provided differently.
class VoxelBlockyLibraryBase : public Resource {
	GDCLASS(VoxelBlockyLibraryBase, Resource)

public:
	static constexpr unsigned int MAX_MODELS = blocky::MAX_MODELS;
	static constexpr unsigned int MAX_FLUIDS = blocky::MAX_FLUIDS;
	static constexpr unsigned int MAX_MATERIALS = blocky::MAX_MATERIALS;

	static constexpr uint32_t NULL_INDEX = 0xFFFFFFFF;

	enum RoundingTaperProfile {
		ROUNDING_TAPER_LINEAR,
		ROUNDING_TAPER_EASE_IN,
		ROUNDING_TAPER_EASE_OUT,
		ROUNDING_TAPER_SMOOTHSTEP
	};

	void set_rounding_amount(float amount);
	float get_rounding_amount() const {
		return _rounding_amount;
	}
	void set_rounding_taper_distance(float distance);
	float get_rounding_taper_distance() const {
		return _rounding_taper_distance;
	}
	void set_rounding_taper_profile(RoundingTaperProfile profile);
	RoundingTaperProfile get_rounding_taper_profile() const {
		return _rounding_taper_profile;
	}

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

	const blocky::BakedLibrary &get_baked_data() const {
		return _baked_data;
	}
	const RWLock &get_baked_data_rw_lock() const {
		return _baked_data_rw_lock;
	}

	Ref<Material> get_material_by_index(unsigned int index) const;
	unsigned int get_material_index_count() const;

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
	// Call only while holding the baked-data write lock.
	void bake_rounding_settings();
	float _rounding_amount = 0.1f;
	float _rounding_taper_distance = 0.4f;
	RoundingTaperProfile _rounding_taper_profile = ROUNDING_TAPER_LINEAR;

	int _legacy_atlas_size = 16;
	bool _needs_baking = true;
	bool _bake_tangents = true;

	// Used in multithread context by the mesher. Don't modify that outside of bake().
	RWLock _baked_data_rw_lock;
	blocky::BakedLibrary _baked_data;
	// One of the entries can be null to represent "The default material". If all non-empty models have materials, there
	// won't be a null entry.
	StdVector<Ref<Material>> _indexed_materials;
};

namespace blocky {

void generate_side_culling_matrix(blocky::BakedLibrary &baked_data);

} // namespace blocky

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelBlockyLibraryBase::RoundingTaperProfile)

#endif // VOXEL_BLOCKY_LIBRARY_BASE_H
