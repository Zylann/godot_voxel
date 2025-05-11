#include "voxel_instance_library_scene_item.h"
#include "../../util/godot/classes/node_3d.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/string/format.h"
#include "voxel_instancer.h"

namespace zylann::voxel {

VoxelInstanceLibrarySceneItem::~VoxelInstanceLibrarySceneItem() {
	clear_pool(0);
}

void VoxelInstanceLibrarySceneItem::set_scene(Ref<PackedScene> scene) {
	if (scene == _scene) {
		return;
	}
	_scene = scene;
	clear_pool(0);
	notify_listeners(IInstanceLibraryItemListener::CHANGE_SCENE);
}

Ref<PackedScene> VoxelInstanceLibrarySceneItem::get_scene() const {
	return _scene;
}

void VoxelInstanceLibrarySceneItem::set_pool_size(const int new_size) {
	const unsigned int checked_new_size = math::clamp<unsigned int>(new_size, 0, MAX_POOL_SIZE);
	if (checked_new_size == _pool_size) {
		return;
	}

	_pool_size = checked_new_size;

	// Remove excess
	clear_pool(_pool_size);
}

int VoxelInstanceLibrarySceneItem::get_pool_size() const {
	return _pool_size;
}

int VoxelInstanceLibrarySceneItem::get_pooled_instance_count() const {
	return _pool.size();
}

void VoxelInstanceLibrarySceneItem::clear_pool(const unsigned int target_count) {
	while (_pool.size() > target_count) {
		Node3D *node = _pool.back();
		_pool.pop_back();
		ZN_ASSERT_CONTINUE(!node->is_inside_tree());
		memdelete(node);
	}
}

static Node3D *create_instance_from_scene(const VoxelInstanceLibrarySceneItem &item) {
	Ref<PackedScene> scene = item.get_scene();

	if (scene.is_null()) {
		ZN_PRINT_ERROR_ONCE(
				format("{} ({}) is missing an attached scene in {}",
					   ZN_CLASS_NAME_C(VoxelInstanceLibrarySceneItem),
					   item.get_item_name(),
					   ZN_CLASS_NAME_C(VoxelInstancer))
		);
		return nullptr;
	}

	Node *node = scene->instantiate();
	if (node == nullptr) {
		// Failure to instantiate
		return nullptr;
	}

	Node3D *node_3d = Object::cast_to<Node3D>(node);
	if (node_3d == nullptr) {
		ZN_PRINT_ERROR_ONCE("Root of scene instance must be derived from Node3D");
		memdelete(node);
		return nullptr;
	}

	return node_3d;
}

Node3D *VoxelInstanceLibrarySceneItem::create_instance() {
	if (_pool.size() > 0) {
		Node3D *node_3d = _pool.back();
		_pool.pop_back();
		return node_3d;
	}
	return create_instance_from_scene(*this);
}

void VoxelInstanceLibrarySceneItem::free_instance(Node3D *node) {
	ZN_ASSERT_RETURN(node != nullptr);

	if (_pool.size() < _pool_size) {
		Node *parent = node->get_parent();
		if (parent != nullptr) {
			parent->remove_child(node);
		}
		_pool.push_back(node);
	} else {
		node->queue_free();
	}
}

#ifdef TOOLS_ENABLED
void VoxelInstanceLibrarySceneItem::get_configuration_warnings(PackedStringArray &warnings) const {
	VoxelInstanceLibraryItem::get_configuration_warnings(warnings);

	if (_scene.is_null()) {
		warnings.append("Scene is not assigned.");
	}
}
#endif

void VoxelInstanceLibrarySceneItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VoxelInstanceLibrarySceneItem::set_scene);
	ClassDB::bind_method(D_METHOD("get_scene"), &VoxelInstanceLibrarySceneItem::get_scene);

	ClassDB::bind_method(D_METHOD("set_pool_size", "max_count"), &VoxelInstanceLibrarySceneItem::set_pool_size);
	ClassDB::bind_method(D_METHOD("get_pool_size"), &VoxelInstanceLibrarySceneItem::get_pool_size);

	ClassDB::bind_method(
			D_METHOD("get_pooled_instance_count"), &VoxelInstanceLibrarySceneItem::get_pooled_instance_count
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, PackedScene::get_class_static()),
			"set_scene",
			"get_scene"
	);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "pool_size"), "set_pool_size", "get_pool_size");
}

} // namespace zylann::voxel
