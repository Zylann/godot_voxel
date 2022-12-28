#ifndef DIRECT_STATIC_BODY_H
#define DIRECT_STATIC_BODY_H

#include "../macros.h"
#include "../non_copyable.h"
#include "classes/shape_3d.h"
#include "direct_mesh_instance.h"

ZN_GODOT_FORWARD_DECLARE(class World3D);

namespace zylann {

// Thin wrapper around static body API
class DirectStaticBody : public zylann::NonCopyable {
public:
	DirectStaticBody();
	~DirectStaticBody();

	void create();
	void destroy();
	bool is_valid() const;
	void set_transform(Transform3D transform);
	void add_shape(Ref<Shape3D> shape);
	void remove_shape(int shape_index);
	Ref<Shape3D> get_shape(int shape_index);
	void set_world(World3D *world);
	void set_shape_enabled(int shape_index, bool disabled);
	void set_attached_object(Object *obj);
	void set_collision_layer(int layer);
	void set_collision_mask(int mask);

	void set_debug(bool enabled, World3D *world);

private:
	RID _body;
	Ref<Shape3D> _shape;
	zylann::DirectMeshInstance _debug_mesh_instance;
};

} // namespace zylann

#endif // DIRECT_STATIC_BODY_H
