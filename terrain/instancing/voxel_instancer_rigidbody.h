#ifndef VOXEL_INSTANCER_RIGIDBODY_H
#define VOXEL_INSTANCER_RIGIDBODY_H

#include "../../util/godot/classes/rigid_body_3d.h"
#include "voxel_instancer.h"

namespace zylann::voxel {

// Provides collision to VoxelInstancer multimesh instances
class VoxelInstancerRigidBody : public RigidBody3D {
	GDCLASS(VoxelInstancerRigidBody, RigidBody3D);

public:
	VoxelInstancerRigidBody();

	void set_data_block_position(Vector3i data_block_position) {
		_data_block_position = data_block_position;
	}

	void set_render_block_index(unsigned int render_block_index) {
		_render_block_index = render_block_index;
	}

	void set_instance_index(int instance_index) {
		_instance_index = instance_index;
	}

	void attach(VoxelInstancer *parent) {
		_parent = parent;
	}

	void detach_and_destroy() {
		_parent = nullptr;
		queue_free();
	}

	int get_library_item_id() const;

	// Note, for this the body must switch to convex shapes
	// void detach_and_become_rigidbody() {
	// 	//...
	// }

protected:
	static void _bind_methods();

	void _notification(int p_what);

private:
	VoxelInstancer *_parent = nullptr;
	Vector3i _data_block_position;
	unsigned int _render_block_index;
	int _instance_index = -1;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_RIGIDBODY_H
