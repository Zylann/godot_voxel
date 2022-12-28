#ifndef DIRECT_MESH_INSTANCE_H
#define DIRECT_MESH_INSTANCE_H

#include "../macros.h"
#include "../non_copyable.h"
#include "classes/mesh.h"
#include "classes/rendering_server.h"

ZN_GODOT_FORWARD_DECLARE(class World3D);

namespace zylann {

// Thin wrapper around VisualServer mesh instance API
class DirectMeshInstance : public NonCopyable {
public:
	DirectMeshInstance();
	DirectMeshInstance(DirectMeshInstance &&src);
	~DirectMeshInstance();

	bool is_valid() const;
	void create();
	void destroy();
	void set_world(World3D *world);
	void set_transform(Transform3D world_transform);
	void set_mesh(Ref<Mesh> mesh);
	void set_material_override(Ref<Material> material);
	void set_visible(bool visible);
	void set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode);
	void set_shader_instance_parameter(StringName key, Variant value);

	// Convenience
	enum GIMode { //
		GI_MODE_DISABLED = 0,
		GI_MODE_BAKED,
		GI_MODE_DYNAMIC,
		_GI_MODE_COUNT
	};

	void set_gi_mode(GIMode mode);

	Ref<Mesh> get_mesh() const;

	// void move_to(DirectMeshInstance &dst);

	void operator=(DirectMeshInstance &&src);

private:
	RID _mesh_instance;
	Ref<Mesh> _mesh;
};

} // namespace zylann

#endif // DIRECT_MESH_INSTANCE_H
