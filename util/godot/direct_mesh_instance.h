#ifndef DIRECT_MESH_INSTANCE_H
#define DIRECT_MESH_INSTANCE_H

#include "../non_copyable.h"
#include "classes/geometry_instance_3d.h"
#include "classes/mesh.h"
#include "classes/rendering_server.h"
#include "macros.h"

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
	void set_gi_mode(GeometryInstance3D::GIMode mode);
	void set_render_layers_mask(int mask);

	Ref<Mesh> get_mesh() const;

	// void move_to(DirectMeshInstance &dst);

	void operator=(DirectMeshInstance &&src);

private:
	RID _mesh_instance;
	Ref<Mesh> _mesh;
};

} // namespace zylann

#endif // DIRECT_MESH_INSTANCE_H
