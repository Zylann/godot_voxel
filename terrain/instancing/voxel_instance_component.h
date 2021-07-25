#ifndef VOXEL_INSTANCE_COMPONENT_H
#define VOXEL_INSTANCE_COMPONENT_H

#include "voxel_instancer.h"
#include <scene/main/node.h>

// Used as child of scene items instanced with VoxelInstancer.
//
// It is needed because such instances are tied with some of the logic in VoxelInstancer.
// The root of a scene could be anything derived from Spatial,
// so offering an API using inheritance on the root node is impractical.
// So instead the component approach is taken.
// If a huge amount of instances is needed, prefer using fast/multimesh instances.
class VoxelInstanceComponent : public Node {
	GDCLASS(VoxelInstanceComponent, Node)
public:
	void mark_modified() {
		ERR_FAIL_COND(_instancer == nullptr);
		_instancer->on_scene_instance_modified(_data_block_position, _render_block_index);
	}

	void detach() {
		ERR_FAIL_COND_MSG(_instancer == nullptr, "Already detached");
		_instancer = nullptr;
	}

	void attach(VoxelInstancer *instancer) {
		ERR_FAIL_COND_MSG(_instancer != nullptr, "Already attached");
		_instancer = instancer;
	}

	// TODO Need to investigate if we need this
	//
	// This must be called by the user from a script if they want the instancer to remember a removal.
	// It may be common to call `queue_free()` on the root (which could be a body or an area) but there doesn't seem to
	// be a reliable way to detect this scenario happens from a child node.
	// `_exit_tree` could happen for different reasons.
	// `unparented` won't happen because the parent is unparented, not the child.
	void detach_as_removed() {
		ERR_FAIL_COND_MSG(_instancer == nullptr, "Already detached");
		_instancer->on_scene_instance_removed(_data_block_position, _render_block_index, _instance_index);
		_instancer = nullptr;
	}

	Variant serialize_state() {
		// TODO Scripting
		return Variant();
	}

	Variant deserialize_state() {
		// TODO Scripting
		return Variant();
	}

	void set_instance_index(int instance_index) {
		_instance_index = instance_index;
	}

	void set_data_block_position(Vector3i data_block_position) {
		_data_block_position = data_block_position;
	}

	void set_render_block_index(unsigned int render_block_index) {
		_render_block_index = render_block_index;
	}

	static VoxelInstanceComponent *find_in(Node *root) {
		ERR_FAIL_COND_V(root == nullptr, nullptr);
		for (int i = 0; i < root->get_child_count(); ++i) {
			Node *child = root->get_child(i);
			VoxelInstanceComponent *cmp = Object::cast_to<VoxelInstanceComponent>(child);
			if (cmp != nullptr) {
				return cmp;
			}
		}
		return nullptr;
	}

protected:
	void _notification(int p_what) {
		switch (p_what) {
				// case NOTIFICATION_PARENTED:
				// 	Spatial *spatial = Object::cast_to<Spatial>(get_parent());
				// 	if (spatial == nullptr) {
				// 		ERR_PRINT("VoxelInstanceComponent must have a parent derived from Spatial");
				// 	}
				// 	break;

			// TODO Optimization: this is also called when we quit the game or destroy the world
			// which can make things a bit slow, but I don't know if it can easily be avoided
			case NOTIFICATION_UNPARENTED:
				// The user could queue_free() that node or its parent in game for some reason,
				// so we have to notify the instancer to remove the instance
				if (_instancer != nullptr) {
					detach_as_removed();
				}
				break;
		}
	}

private:
	static void _bind_methods() {
		// TODO Scripting
	}

	VoxelInstancer *_instancer = nullptr;
	Vector3i _data_block_position;
	unsigned int _render_block_index;
	int _instance_index = -1;
};

#endif // VOXEL_INSTANCE_COMPONENT_H
