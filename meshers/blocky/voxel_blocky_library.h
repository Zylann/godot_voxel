#ifndef VOXEL_BLOCKY_LIBRARY_H
#define VOXEL_BLOCKY_LIBRARY_H

#include "../../util/containers/std_vector.h"
#include "voxel_blocky_library_base.h"

namespace zylann::voxel {

// Library exposing every model in a simple array. Indices in the array correspond to voxel data.
// Rotations and variants have to be setup manually as separate models. You may use this library if your models are
// simple, or if you want to organize your own system of voxel types.
//
// Should have been named `VoxelBlockyModelLibrary` or `VoxelBlockyLibrarySimple`, but the name was kept for
// compatibility with previous versions.
class VoxelBlockyLibrary : public VoxelBlockyLibraryBase {
	GDCLASS(VoxelBlockyLibrary, VoxelBlockyLibraryBase)

public:
	VoxelBlockyLibrary();
	~VoxelBlockyLibrary();

	void load_default() override;
	void clear() override;

	void bake() override;

	int get_model_index_from_resource_name(String resource_name) const;

	// Convenience method that returns the index of the added model
	int add_model(Ref<VoxelBlockyModel> model);

	//-------------------------
	// Internal use

	// inline bool has_model(unsigned int id) const {
	// 	return id < _voxel_models.size() && _voxel_models[id].is_valid();
	// }

	// unsigned int get_model_count() const;

	// inline const VoxelBlockyModel &get_model_const(unsigned int id) const {
	// 	const Ref<VoxelBlockyModel> &model = _voxel_models[id];
	// 	ZN_ASSERT(model.is_valid());
	// 	return **model;
	// }

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

private:
	Ref<VoxelBlockyModel> _b_get_model(unsigned int id) const;

	TypedArray<VoxelBlockyModel> _b_get_models() const;
	void _b_set_models(TypedArray<VoxelBlockyModel> models);

	int _b_deprecated_get_voxel_index_from_name(String p_name) const;

	bool _set(const StringName &p_name, const Variant &p_value);

	static void _bind_methods();

private:
	// Indices matter, they correspond to voxel data
	StdVector<Ref<VoxelBlockyModel>> _voxel_models;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_LIBRARY_H
