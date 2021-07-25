#ifndef VOXEL_INSTANCER_RIGIDBODY_H
#define VOXEL_INSTANCER_RIGIDBODY_H

#include "voxel_instancer.h"
#include <scene/3d/physics_body.h>

// Provides collision to VoxelInstancer multimesh instances
class VoxelInstancerRigidBody : public RigidBody {
	GDCLASS(VoxelInstancerRigidBody, RigidBody);

public:
	VoxelInstancerRigidBody() {
		set_mode(RigidBody::MODE_STATIC);
	}

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
		queue_delete();
	}

	// Note, for this the body must switch to convex shapes
	// void detach_and_become_rigidbody() {
	// 	//...
	// }

protected:
	void _notification(int p_what) {
		switch (p_what) {
			// TODO Optimization: this is also called when we quit the game or destroy the world
			// which can make things a bit slow, but I don't know if it can easily be avoided
			case NOTIFICATION_UNPARENTED:
				// The user could queue_free() that node in game,
				// so we have to notify the instancer to remove the multimesh instance and pointer
				if (_parent != nullptr) {
					_parent->on_body_removed(_data_block_position, _render_block_index, _instance_index);
					_parent = nullptr;
				}
				break;
		}
	}

private:
	VoxelInstancer *_parent = nullptr;
	Vector3i _data_block_position;
	unsigned int _render_block_index;
	int _instance_index = -1;
};

#endif // VOXEL_INSTANCER_RIGIDBODY_H
