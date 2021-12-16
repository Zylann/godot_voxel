#ifndef DIRECT_MESH_INSTANCE_H
#define DIRECT_MESH_INSTANCE_H

#include <core/templates/rid.h>
#include <scene/resources/mesh.h>

class World3D;

// Thin wrapper around VisualServer mesh instance API
class DirectMeshInstance {
public:
	DirectMeshInstance();
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
	// void set_use_baked_light(bool enable);
	// void set_use_dynamic_gi(bool enable);

	// Convenience
	enum GIMode { //
		GI_MODE_DISABLED = 0,
		GI_MODE_BAKED,
		GI_MODE_DYNAMIC,
		_GI_MODE_COUNT
	};

	void set_gi_mode(GIMode mode);

	Ref<Mesh> get_mesh() const;

private:
	RID _mesh_instance;
	Ref<Mesh> _mesh;
};

#endif // DIRECT_MESH_INSTANCE_H
