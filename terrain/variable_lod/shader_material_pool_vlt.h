#ifndef VOXEL_SHADER_MATERIAL_POOL_VLT_H
#define VOXEL_SHADER_MATERIAL_POOL_VLT_H

#include "../../util/godot/shader_material_pool.h"

namespace zylann::voxel {

class ShaderMaterialPoolVLT : public zylann::godot::ShaderMaterialPool {
public:
	void recycle(Ref<ShaderMaterial> material);
};

} // namespace zylann::voxel

#endif // VOXEL_SHADER_MATERIAL_POOL_VLT_H
