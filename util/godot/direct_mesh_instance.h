#ifndef DIRECT_MESH_INSTANCE_H
#define DIRECT_MESH_INSTANCE_H

#include "../non_copyable.h"
#include <core/templates/rid.h>
#include <scene/resources/mesh.h>

class World3D;

// Thin wrapper around VisualServer mesh instance API
class DirectMeshInstance : public zylann::NonCopyable {
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

private:
	RID _mesh_instance;
	Ref<Mesh> _mesh;
};

#endif // DIRECT_MESH_INSTANCE_H
