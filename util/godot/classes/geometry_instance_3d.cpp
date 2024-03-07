#include "geometry_instance_3d.h"
#include "rendering_server.h"

namespace zylann::godot {

const char *const CAST_SHADOW_ENUM_HINT_STRING = "Off,On,Double-Sided,Shadows Only";
const char *const GI_MODE_ENUM_HINT_STRING = "Disabled,Static (VoxelGI/SDFGI/LightmapGI),Dynamic (VoxelGI only)";

void set_geometry_instance_gi_mode(RID rid, GeometryInstance3D::GIMode mode) {
	ERR_FAIL_COND(!rid.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();

	bool baked_light;
	bool dynamic_gi;

	switch (mode) {
		case GeometryInstance3D::GI_MODE_DISABLED:
			baked_light = false;
			dynamic_gi = false;
			break;
		case GeometryInstance3D::GI_MODE_STATIC:
			baked_light = true;
			dynamic_gi = false;
			break;
		case GeometryInstance3D::GI_MODE_DYNAMIC:
			baked_light = false;
			dynamic_gi = true;
			break;
		default:
			ERR_FAIL_MSG("Unexpected GIMode");
			return;
	}

	vs.instance_geometry_set_flag(rid, RenderingServer::INSTANCE_FLAG_USE_BAKED_LIGHT, baked_light);
	vs.instance_geometry_set_flag(rid, RenderingServer::INSTANCE_FLAG_USE_DYNAMIC_GI, dynamic_gi);
}

} // namespace zylann::godot
