#ifndef DIRECT_MULTIMESH_INSTANCE_H
#define DIRECT_MULTIMESH_INSTANCE_H

#include "../span.h"

#include <core/rid.h>
#include <scene/resources/multimesh.h>

class World;

// Thin wrapper around VisualServer multimesh instance API
class DirectMultiMeshInstance {
public:
	DirectMultiMeshInstance();
	~DirectMultiMeshInstance();

	void create();
	void destroy();
	bool is_valid() const;
	void set_world(World *world);
	void set_multimesh(Ref<MultiMesh> multimesh);
	Ref<MultiMesh> get_multimesh() const;
	void set_transform(Transform world_transform);
	void set_visible(bool visible);
	void set_material_override(Ref<Material> material);
	void set_cast_shadows_setting(VisualServer::ShadowCastingSetting mode);

	static PoolRealArray make_transform_3d_bulk_array(Span<const Transform> transforms);

private:
	RID _multimesh_instance;
	Ref<MultiMesh> _multimesh;
};

#endif // DIRECT_MULTIMESH_INSTANCE_H
