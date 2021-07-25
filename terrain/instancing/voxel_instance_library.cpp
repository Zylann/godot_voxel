#include "voxel_instance_library.h"
#include "voxel_instancer.h"

#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>

VoxelInstanceLibrary::~VoxelInstanceLibrary() {
	for_each_item([this](int id, VoxelInstanceLibraryItemBase &item) {
		item.remove_listener(this, id);
	});
}

int VoxelInstanceLibrary::get_next_available_id() {
	if (_items.empty()) {
		return 1;
	} else {
		return _items.back()->key() + 1;
	}
}

void VoxelInstanceLibrary::add_item(int id, Ref<VoxelInstanceLibraryItemBase> item) {
	ERR_FAIL_COND(_items.has(id));
	ERR_FAIL_COND(id < 0 || id >= MAX_ID);
	ERR_FAIL_COND(item.is_null());
	_items.insert(id, item);
	item->add_listener(this, id);
	notify_listeners(id, VoxelInstanceLibraryItemBase::CHANGE_ADDED);
	_change_notify();
}

void VoxelInstanceLibrary::remove_item(int id) {
	Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);
	ERR_FAIL_COND(E == nullptr);
	Ref<VoxelInstanceLibraryItemBase> item = E->value();
	if (item.is_valid()) {
		item->remove_listener(this, id);
	}
	_items.erase(E);
	notify_listeners(id, VoxelInstanceLibraryItemBase::CHANGE_REMOVED);
	_change_notify();
}

void VoxelInstanceLibrary::clear() {
	for_each_item([this](int id, const VoxelInstanceLibraryItemBase &item) {
		notify_listeners(id, VoxelInstanceLibraryItemBase::CHANGE_REMOVED);
	});
	_items.clear();
	_change_notify();
}

int VoxelInstanceLibrary::find_item_by_name(String name) const {
	for (Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.front(); E != nullptr; E = E->next()) {
		const Ref<VoxelInstanceLibraryItemBase> &item = E->value();
		ERR_FAIL_COND_V(item.is_null(), -1);
		if (item->get_name() == name) {
			return E->key();
		}
	}
	return -1;
}

int VoxelInstanceLibrary::get_item_count() const {
	return _items.size();
}

Ref<VoxelInstanceLibraryItemBase> VoxelInstanceLibrary::_b_get_item(int id) {
	Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);
	Ref<VoxelInstanceLibraryItemBase> item;
	if (E != nullptr) {
		item = E->value();
	}
	return item;
}

VoxelInstanceLibraryItemBase *VoxelInstanceLibrary::get_item(int id) {
	Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);
	if (E != nullptr) {
		Ref<VoxelInstanceLibraryItemBase> &item = E->value();
		ERR_FAIL_COND_V(item.is_null(), nullptr);
		return *item;
	}
	return nullptr;
}

const VoxelInstanceLibraryItemBase *VoxelInstanceLibrary::get_item_const(int id) const {
	const Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);
	if (E != nullptr) {
		const Ref<VoxelInstanceLibraryItemBase> &item = E->value();
		ERR_FAIL_COND_V(item.is_null(), nullptr);
		return *item;
	}
	return nullptr;
}

void VoxelInstanceLibrary::on_library_item_changed(int id, VoxelInstanceLibraryItemBase::ChangeType change) {
	notify_listeners(id, change);
}

void VoxelInstanceLibrary::notify_listeners(int item_id, VoxelInstanceLibraryItemBase::ChangeType change) {
	for (int i = 0; i < _listeners.size(); ++i) {
		IListener *listener = _listeners[i];
		listener->on_library_item_changed(item_id, change);
	}
}

void VoxelInstanceLibrary::add_listener(IListener *listener) {
	ERR_FAIL_COND(_listeners.find(listener) != -1);
	_listeners.push_back(listener);
}

void VoxelInstanceLibrary::remove_listener(IListener *listener) {
	const int i = _listeners.find(listener);
	ERR_FAIL_COND(i == -1);
	_listeners.remove(i);
}

bool VoxelInstanceLibrary::_set(const StringName &p_name, const Variant &p_value) {
	const String name = p_name;

	if (name.begins_with("item_")) {
		const int id = name.substr(5).to_int();

		Ref<VoxelInstanceLibraryItemBase> item = p_value;
		ERR_FAIL_COND_V_MSG(item.is_null(), false, "Setting a null item is not allowed");

		Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);

		if (E == nullptr) {
			add_item(id, item);

		} else {
			// Replace
			if (E->value() != item) {
				Ref<VoxelInstanceLibraryItemBase> old_item = E->value();
				if (old_item.is_valid()) {
					old_item->remove_listener(this, id);
					notify_listeners(id, VoxelInstanceLibraryItemBase::CHANGE_REMOVED);
				}
				E->value() = item;
				item->add_listener(this, id);
				notify_listeners(id, VoxelInstanceLibraryItemBase::CHANGE_ADDED);
			}
		}

		return true;
	}
	return false;
}

bool VoxelInstanceLibrary::_get(const StringName &p_name, Variant &r_ret) const {
	const String name = p_name;
	if (name.begins_with("item_")) {
		const int id = name.substr(5).to_int();
		const Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.find(id);
		if (E != nullptr) {
			r_ret = E->value();
			return true;
		}
	}
	return false;
}

void VoxelInstanceLibrary::_get_property_list(List<PropertyInfo> *p_list) const {
	for (Map<int, Ref<VoxelInstanceLibraryItemBase>>::Element *E = _items.front(); E != nullptr; E = E->next()) {
		const String name = "item_" + itos(E->key());
		p_list->push_back(
				PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "VoxelInstanceLibraryItemBase"));
	}
}

void VoxelInstanceLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_item", "id"), &VoxelInstanceLibrary::add_item);
	ClassDB::bind_method(D_METHOD("remove_item", "id"), &VoxelInstanceLibrary::remove_item);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelInstanceLibrary::clear);
	ClassDB::bind_method(D_METHOD("find_item_by_name", "name"), &VoxelInstanceLibrary::find_item_by_name);
	ClassDB::bind_method(D_METHOD("get_item", "id"), &VoxelInstanceLibrary::_b_get_item);

	BIND_CONSTANT(MAX_ID);
}
