#include "shader_material_pool_vlt.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/texture_2d.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

void ShaderMaterialPoolVLT::recycle(Ref<ShaderMaterial> material) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(material.is_valid());

	const VoxelStringNames &sn = VoxelStringNames::get_singleton();

	// Reset textures to avoid hoarding them in the pool
	material->set_shader_parameter(sn.u_voxel_normalmap_atlas, Ref<Texture2D>());
	material->set_shader_parameter(sn.u_voxel_cell_lookup, Ref<Texture2D>());
	material->set_shader_parameter(sn.u_voxel_virtual_texture_offset_scale, Vector4(0, 0, 0, 1));
	// TODO Would be nice if we repurposed `u_transition_mask` to store extra flags.
	// Here we exploit cell_size==0 as "there is no virtual normalmaps on this block"
	material->set_shader_parameter(sn.u_voxel_cell_size, 0.f);
	material->set_shader_parameter(sn.u_voxel_virtual_texture_fade, 0.f);

	material->set_shader_parameter(sn.u_transition_mask, 0);
	material->set_shader_parameter(sn.u_lod_fade, Vector2(0.0, 0.0));

	zylann::godot::ShaderMaterialPool::recycle(material);
}

} // namespace zylann::voxel
