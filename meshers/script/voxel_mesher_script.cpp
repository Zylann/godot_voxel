#include "voxel_mesher_script.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

VoxelMesherScript::VoxelMesherScript() {
	// Default padding
	set_padding(1, 1);
}

VoxelMesherScript::~VoxelMesherScript() {
	for (VoxelMesherContext *context : _context_pool) {
		memdelete(context);
	}
}

void VoxelMesherScript::build(VoxelMesherOutput &output, const VoxelMesherInput &input) {
	ZN_PROFILE_SCOPE();

	VoxelMesherContext *context = get_or_create_context();

	context->input = &input;

	if (!GDVIRTUAL_CALL(_build, context)) {
		WARN_PRINT_ONCE("VoxelMesherScript::_build is unimplemented!");
	}

	return_context(context);
}

Ref<Material> VoxelMesherScript::get_material_by_index(unsigned int i) const {
	Ref<Material> material;
	if (!GDVIRTUAL_CALL(_get_material_by_index, i, material)) {
		WARN_PRINT_ONCE("VoxelMesherScript::_get_material_by_index is unimplemented!");
	}
	return material;
}

unsigned int VoxelMesherScript::get_material_index_count() const {
	int count = 0;
	if (!GDVIRTUAL_CALL(_get_material_index_count, count)) {
		WARN_PRINT_ONCE("VoxelMesherScript::_get_material_index_count is unimplemented!");
	}
	ZN_ASSERT_RETURN_V(count >= 0, 0);
	return count;
}

VoxelMesherContext *VoxelMesherScript::get_or_create_context() {
	{
		MutexLock(_context_pool_mutex);
		if (_context_pool.size() != 0) {
			VoxelMesherContext *context = _context_pool.back();
			_context_pool.pop_back();
			return context;
		}
	}
	return memnew(VoxelMesherContext);
}

void VoxelMesherScript::return_context(VoxelMesherContext *context) {
	MutexLock mlock(_context_pool_mutex);
	_context_pool.push_back(context);
}

void VoxelMesherScript::_b_set_padding(const int p_negative_padding, const int p_positive_padding) {
	// Check for reasonable padding
	ZN_ASSERT_RETURN(p_negative_padding >= 0 && p_negative_padding <= 2);
	ZN_ASSERT_RETURN(p_positive_padding >= 0 && p_positive_padding <= 2);
	set_padding(p_negative_padding, p_positive_padding);
}

void VoxelMesherScript::_bind_methods() {
	GDVIRTUAL_BIND(_build, "context");
	GDVIRTUAL_BIND(_get_material_by_index, "index");
	GDVIRTUAL_BIND(_get_material_index_count);
}

} // namespace zylann::voxel
