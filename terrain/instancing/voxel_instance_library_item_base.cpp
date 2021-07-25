#include "voxel_instance_library_item.h"
#include "voxel_instancer.h"

#include <core/core_string_names.h>
#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>

void VoxelInstanceLibraryItemBase::set_item_name(String name) {
	_name = name;
}

String VoxelInstanceLibraryItemBase::get_item_name() const {
	return _name;
}

void VoxelInstanceLibraryItemBase::set_lod_index(int lod) {
	ERR_FAIL_COND(lod < 0 || lod >= VoxelInstancer::MAX_LOD);
	if (_lod_index == lod) {
		return;
	}
	_lod_index = lod;
	notify_listeners(CHANGE_LOD_INDEX);
}

int VoxelInstanceLibraryItemBase::get_lod_index() const {
	return _lod_index;
}

void VoxelInstanceLibraryItemBase::set_generator(Ref<VoxelInstanceGenerator> generator) {
	if (_generator == generator) {
		return;
	}
	if (_generator.is_valid()) {
		_generator->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_generator_changed");
	}
	_generator = generator;
	if (_generator.is_valid()) {
		_generator->connect(CoreStringNames::get_singleton()->changed, this, "_on_generator_changed");
	}
	notify_listeners(CHANGE_GENERATOR);
}

Ref<VoxelInstanceGenerator> VoxelInstanceLibraryItemBase::get_generator() const {
	return _generator;
}

void VoxelInstanceLibraryItemBase::set_persistent(bool persistent) {
	_persistent = persistent;
}

bool VoxelInstanceLibraryItemBase::is_persistent() const {
	return _persistent;
}

void VoxelInstanceLibraryItemBase::add_listener(IListener *listener, int id) {
	ListenerSlot slot;
	slot.listener = listener;
	slot.id = id;
	ERR_FAIL_COND(_listeners.find(slot) != -1);
	_listeners.push_back(slot);
}

void VoxelInstanceLibraryItemBase::remove_listener(IListener *listener, int id) {
	ListenerSlot slot;
	slot.listener = listener;
	slot.id = id;
	int i = _listeners.find(slot);
	ERR_FAIL_COND(i == -1);
	_listeners.remove(i);
}

void VoxelInstanceLibraryItemBase::notify_listeners(ChangeType change) {
	for (int i = 0; i < _listeners.size(); ++i) {
		ListenerSlot &slot = _listeners.write[i];
		slot.listener->on_library_item_changed(slot.id, change);
	}
}

void VoxelInstanceLibraryItemBase::_on_generator_changed() {
	notify_listeners(CHANGE_GENERATOR);
}

void VoxelInstanceLibraryItemBase::_bind_methods() {
	// Can't be just "set_name" because Resource already defines that, despite being for a `resource_name` property
	ClassDB::bind_method(D_METHOD("set_item_name", "name"), &VoxelInstanceLibraryItemBase::set_item_name);
	ClassDB::bind_method(D_METHOD("get_item_name"), &VoxelInstanceLibraryItemBase::get_item_name);

	ClassDB::bind_method(D_METHOD("set_lod_index", "lod"), &VoxelInstanceLibraryItemBase::set_lod_index);
	ClassDB::bind_method(D_METHOD("get_lod_index"), &VoxelInstanceLibraryItemBase::get_lod_index);

	ClassDB::bind_method(D_METHOD("set_generator", "generator"), &VoxelInstanceLibraryItemBase::set_generator);
	ClassDB::bind_method(D_METHOD("get_generator"), &VoxelInstanceLibraryItemBase::get_generator);

	ClassDB::bind_method(D_METHOD("set_persistent", "persistent"), &VoxelInstanceLibraryItemBase::set_persistent);
	ClassDB::bind_method(D_METHOD("is_persistent"), &VoxelInstanceLibraryItemBase::is_persistent);

	ClassDB::bind_method(D_METHOD("_on_generator_changed"), &VoxelInstanceLibraryItemBase::_on_generator_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_item_name", "get_item_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_index", PROPERTY_HINT_RANGE, "0,8,1"),
			"set_lod_index", "get_lod_index");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "generator", PROPERTY_HINT_RESOURCE_TYPE, "VoxelInstanceGenerator"),
			"set_generator", "get_generator");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "persistent"), "set_persistent", "is_persistent");
}
