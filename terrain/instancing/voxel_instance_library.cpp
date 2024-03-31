#include "voxel_instance_library.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/profiling.h"
#include "voxel_instancer.h"
#include <algorithm>
#ifdef ZN_GODOT_EXTENSION
#include "../../util/godot/core/array.h"
#include "../../util/string/format.h"
#endif

namespace zylann::voxel {

VoxelInstanceLibrary::~VoxelInstanceLibrary() {
	for_each_item([this](int id, VoxelInstanceLibraryItem &item) { //
		item.remove_listener(this, id);
	});
}

int VoxelInstanceLibrary::get_next_available_id() {
	if (_items.empty()) {
		return 1;
	} else {
		// Get highest key and increment it
		return _items.rbegin()->first + 1;
	}
}

void VoxelInstanceLibrary::add_item(int p_id, Ref<VoxelInstanceLibraryItem> item) {
	ERR_FAIL_COND(item.is_null());
	ERR_FAIL_COND(p_id < 0 || p_id >= MAX_ID);
	const unsigned int id = p_id;
	ERR_FAIL_COND_MSG(_items.find(id) != _items.end(), "An item with the same ID is already registered");
	_items.insert({ id, item });
	item->add_listener(this, id);

	// This is also called when the resource is loaded, so do this iteratively instead of updating all packed items
	Ref<VoxelInstanceGenerator> generator = item->get_generator();
	if (generator.is_valid()) {
		PackedItems::Lod &lod = _packed_items.lods[item->get_lod_index()];
		MutexLock mlock(_packed_items.mutex);
		if (!contains(lod.items, [id](const PackedItem &existing_item) { return existing_item.id == id; })) {
			PackedItem packed_item;
			packed_item.id = id;
			packed_item.generator = generator;
			lod.items.push_back(packed_item);
		}
	}

	notify_listeners(id, VoxelInstanceLibraryItem::CHANGE_ADDED);
	notify_property_list_changed();
}

void VoxelInstanceLibrary::remove_item(int p_id) {
	ZN_ASSERT_RETURN(p_id < 0 || p_id >= MAX_ID);
	const unsigned int id = p_id;
	auto it = _items.find(id);
	ERR_FAIL_COND_MSG(it == _items.end(), "Cannot remove unregistered item");
	Ref<VoxelInstanceLibraryItem> item = it->second;
	if (item.is_valid()) {
		item->remove_listener(this, id);
	}
	_items.erase(it);

	// This is also called when the resource is loaded, so do this iteratively instead of updating all packed items
	PackedItems::Lod &lod = _packed_items.lods[item->get_lod_index()];
	{
		MutexLock mlock(_packed_items.mutex);
		size_t index;
		if (find(lod.items, index, [id](const PackedItem &pi) { return pi.id == id; })) {
			lod.items.erase(lod.items.begin() + index);
		}
	}

	notify_listeners(id, VoxelInstanceLibraryItem::CHANGE_REMOVED);
	notify_property_list_changed();
}

void VoxelInstanceLibrary::clear() {
	for_each_item([this](int id, const VoxelInstanceLibraryItem &item) {
		notify_listeners(id, VoxelInstanceLibraryItem::CHANGE_REMOVED);
	});

	_items.clear();

	notify_property_list_changed();
}

int VoxelInstanceLibrary::find_item_by_name(String p_name) const {
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		const Ref<VoxelInstanceLibraryItem> &item = it->second;
		ERR_FAIL_COND_V(item.is_null(), -1);
		if (item->get_item_name() == p_name) {
			return it->first;
		}
	}
	return -1;
}

int VoxelInstanceLibrary::get_item_count() const {
	return _items.size();
}

Ref<VoxelInstanceLibraryItem> VoxelInstanceLibrary::_b_get_item(int id) const {
	auto it = _items.find(id);
	Ref<VoxelInstanceLibraryItem> item;
	if (it != _items.end()) {
		item = it->second;
	}
	return item;
}

VoxelInstanceLibraryItem *VoxelInstanceLibrary::get_item(int id) {
	auto it = _items.find(id);
	if (it != _items.end()) {
		Ref<VoxelInstanceLibraryItem> &item = it->second;
		ERR_FAIL_COND_V(item.is_null(), nullptr);
		return *item;
	}
	return nullptr;
}

const VoxelInstanceLibraryItem *VoxelInstanceLibrary::get_item_const(int id) const {
	auto it = _items.find(id);
	if (it != _items.end()) {
		const Ref<VoxelInstanceLibraryItem> &item = it->second;
		ERR_FAIL_COND_V(item.is_null(), nullptr);
		return *item;
	}
	return nullptr;
}

void VoxelInstanceLibrary::on_library_item_changed(int id, VoxelInstanceLibraryItem::ChangeType change) {
	switch (change) {
		// These changes will be reported after the resource is loaded and should be rare in-game, or occur in the
		// editor, so we can do a simpler bulk update
		case VoxelInstanceLibraryItem::CHANGE_GENERATOR:
		case VoxelInstanceLibraryItem::CHANGE_LOD_INDEX:
			update_packed_items();
			break;
		default:
			break;
	}

	notify_listeners(id, change);
}

void VoxelInstanceLibrary::notify_listeners(int item_id, VoxelInstanceLibraryItem::ChangeType change) {
	for (unsigned int i = 0; i < _listeners.size(); ++i) {
		IListener *listener = _listeners[i];
		listener->on_library_item_changed(item_id, change);
	}
}

void VoxelInstanceLibrary::add_listener(IListener *listener) {
	ERR_FAIL_COND(std::find(_listeners.begin(), _listeners.end(), listener) != _listeners.end());
	_listeners.push_back(listener);
}

void VoxelInstanceLibrary::remove_listener(IListener *listener) {
	auto it = std::find(_listeners.begin(), _listeners.end(), listener);
	ERR_FAIL_COND(it == _listeners.end());
	_listeners.erase(it);
}

void VoxelInstanceLibrary::get_packed_items_at_lod(StdVector<PackedItem> &out_items, unsigned int lod_index) const {
	const PackedItems::Lod &lod = _packed_items.lods[lod_index];
	MutexLock mlock(_packed_items.mutex);
	append_array(out_items, lod.items);
}

void VoxelInstanceLibrary::update_packed_items() {
	ZN_PROFILE_SCOPE();

	// Yet another candidate for a post-resource-loading callback.
	// TODO Maybe we could solve this if items were stored in an exposed TypedArray<Item>? We'd do it in the setter?
	// That doesn't solve cases where items themselves change though

	// This should be called rarely. Main use case is in the editor, and eventually in-game editing?

	PackedItems &packed_items = _packed_items;
	MutexLock mlock(_packed_items.mutex);

	for (PackedItems::Lod &lod : packed_items.lods) {
		lod.items.clear();
	}

	for_each_item([&packed_items](int id, const VoxelInstanceLibraryItem &item) {
		const unsigned int lod_index = item.get_lod_index();
		PackedItems::Lod &lod = packed_items.lods[lod_index];
		PackedItem packed_item;
		packed_item.generator = item.get_generator();
		if (packed_item.generator.is_null()) {
			return;
		}
		packed_item.id = id;
		lod.items.push_back(packed_item);
	});

	packed_items.needs_update = false;
}

#ifdef TOOLS_ENABLED

void VoxelInstanceLibrary::get_configuration_warnings(PackedStringArray &warnings) const {
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		Ref<VoxelInstanceLibraryItem> item = it->second;
		ZN_ASSERT_CONTINUE(item.is_valid());
		godot::get_resource_configuration_warnings(**item, warnings,
				[&it]() { //
					return String("Item {0} (\"{1}\"): ").format(varray(it->first, it->second->get_item_name()));
				});
	}
}

#endif

void VoxelInstanceLibrary::set_item(int id, Ref<VoxelInstanceLibraryItem> item) {
	ZN_ASSERT_RETURN(item.is_valid());

	auto it = _items.find(id);

	if (it == _items.end()) {
		add_item(id, item);

	} else {
		// Replace
		if (it->second != item) {
			Ref<VoxelInstanceLibraryItem> old_item = it->second;
			if (old_item.is_valid()) {
				old_item->remove_listener(this, id);
				notify_listeners(id, VoxelInstanceLibraryItem::CHANGE_REMOVED);
			}
			it->second = item;

			item->add_listener(this, id);
			notify_listeners(id, VoxelInstanceLibraryItem::CHANGE_ADDED);
		}
	}
}

bool VoxelInstanceLibrary::_set(const StringName &p_name, const Variant &p_value) {
	const String property_name = p_name;
	if (property_name.begins_with("item_")) {
		const int id = property_name.substr(5).to_int();
		Ref<VoxelInstanceLibraryItem> item = p_value;
		ERR_FAIL_COND_V_MSG(item.is_null(), false, "Setting a null item is not allowed");
		set_item(id, item);
		return true;
	}
	return false;
}

bool VoxelInstanceLibrary::_get(const StringName &p_name, Variant &r_ret) const {
	const String property_name = p_name;
	if (property_name.begins_with("item_")) {
		const int id = property_name.substr(5).to_int();
		auto it = _items.find(id);
		if (it != _items.end()) {
			r_ret = it->second;
			return true;
		}
	}
	return false;
}

void VoxelInstanceLibrary::_get_property_list(List<PropertyInfo> *p_list) const {
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		const String property_name = "item_" + itos(it->first);
		p_list->push_back(PropertyInfo(Variant::OBJECT, property_name, PROPERTY_HINT_RESOURCE_TYPE,
				VoxelInstanceLibraryItem::get_class_static()));
	}
}

PackedInt32Array VoxelInstanceLibrary::_b_get_all_item_ids() const {
	PackedInt32Array ids;
	ids.resize(_items.size());
	// Doing this because in GDExtension builds assigning items has different syntax than modules... and it's faster
	int *ids_w = ids.ptrw();
	int i = 0;
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		ids_w[i] = it->first;
		++i;
	}
	return ids;
}

void VoxelInstanceLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_item", "id", "item"), &VoxelInstanceLibrary::add_item);
	ClassDB::bind_method(D_METHOD("remove_item", "id"), &VoxelInstanceLibrary::remove_item);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelInstanceLibrary::clear);
	ClassDB::bind_method(D_METHOD("find_item_by_name", "name"), &VoxelInstanceLibrary::find_item_by_name);
	ClassDB::bind_method(D_METHOD("get_item", "id"), &VoxelInstanceLibrary::_b_get_item);
	ClassDB::bind_method(D_METHOD("get_all_item_ids"), &VoxelInstanceLibrary::_b_get_all_item_ids);

	BIND_CONSTANT(MAX_ID);
}

} // namespace zylann::voxel
