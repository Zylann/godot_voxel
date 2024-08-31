#include "voxel_instance_library.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/typed_array.h"
#include "../../util/profiling.h"
#include "array_utils.h"
#include "instance_library_listener.h"
#include "voxel_instance_emitter.h"
#include "voxel_instance_library_item.h"

#include <algorithm>
#ifdef ZN_GODOT_EXTENSION
#include "../../util/godot/core/array.h"
#include "../../util/string/format.h"
#endif

namespace zylann::voxel {

VoxelInstanceLibrary::~VoxelInstanceLibrary() {
	for_each_layer([this](const int unused_id, VoxelInstanceLibraryItem *item) { //
		item->remove_listener(this);
	});
	for_each_emitter([this](VoxelInstanceEmitter &emitter) { //
		emitter.remove_listener(this);
	});
}

#ifdef TOOLS_ENABLED

void VoxelInstanceLibrary::get_configuration_warnings(PackedStringArray &warnings) const {
	if (_emitters.size() == 0) {
		warnings.append("No emitters registered");
	}
	for (unsigned int emitter_index = 0; emitter_index < _emitters.size(); ++emitter_index) {
		const Ref<VoxelInstanceEmitter> &emitter = _emitters[emitter_index];
		if (emitter.is_valid()) {
			godot::get_resource_configuration_warnings(**emitter, warnings, [emitter_index, &emitter]() {
				return String("Emitter {0} (\"{1}\"): ").format(varray(emitter_index, emitter->get_name()));
			});
		} else {
			warnings.append(String("Emitter {0} is null").format(varray(emitter_index)));
		}
	}
}

#endif

const VoxelInstanceLibraryItem *VoxelInstanceLibrary::get_item_by_layer_id(const int id) const {
	auto it = _layer_id_to_item.find(id);
	ZN_ASSERT_RETURN_V(it != _layer_id_to_item.end(), nullptr);
	return it->second.ptr();
}

int VoxelInstanceLibrary::get_layer_id_from_emitter_and_item(
		const VoxelInstanceLibraryItem *item,
		const VoxelInstanceEmitter *emitter
) const {
	const LayerKey key{ item, emitter };
	auto it = _item_to_layer_id.find(key);
	if (it == _item_to_layer_id.end()) {
		return -1;
	}
	return it->second;
}

void VoxelInstanceLibrary::add_listener(IInstanceLibraryListener *listener) {
	ZN_ASSERT_RETURN(listener != nullptr);
	ZN_ASSERT_RETURN(!contains(_listeners, listener));
	_listeners.push_back(listener);
}

void VoxelInstanceLibrary::remove_listener(IInstanceLibraryListener *listener) {
	auto it = std::find(_listeners.begin(), _listeners.end(), listener);
	ZN_ASSERT_RETURN(it != _listeners.end());
	_listeners.erase(it);
}

Span<const Ref<VoxelInstanceEmitter>> VoxelInstanceLibrary::get_emitters() const {
	return to_span_const(_emitters);
}

unsigned int VoxelInstanceLibrary::get_registered_item_count() const {
	return _layer_id_to_item.size();
}

int VoxelInstanceLibrary::get_next_available_layer_id() const {
	if (_layer_id_to_item.size() == 0) {
		return 1;
	}
	return _layer_id_to_item.rbegin()->first + 1;
}

// TODO Identifying items by layer ID is only really useful for multimeshes, scenes could be tagged with it instead.
// This means all scenes could share a same special layer ID per LOD. We may want to refactor that later...

void VoxelInstanceLibrary::register_item(Ref<VoxelInstanceLibraryItem> item, const VoxelInstanceEmitter *emitter) {
	const LayerKey key{ item.ptr(), emitter };
	if (_item_to_layer_id.find(key) != _item_to_layer_id.end()) {
		// Already registered
		return;
	}
	const int id = get_next_available_layer_id();
	_item_to_layer_id.insert({ key, id });
	_layer_id_to_item.insert({ id, item });

	for (IInstanceLibraryListener *listener : _listeners) {
		listener->on_library_item_registered(id, emitter->get_lod_index());
	}

	_packed_data_need_update = true;
}

// bool any_emitter_has_item(
// 		const VoxelInstanceLibrary &library,
// 		const Ref<VoxelInstanceLibraryItem> &item,
// 		uint8_t lod_index
// ) {
// 	for (const Ref<VoxelInstanceEmitter> &emitter : library.get_emitters()) {
// 		if (emitter.is_null()) {
// 			continue;
// 		}
// 		if (emitter->get_lod_index() != lod_index) {
// 			continue;
// 		}
// 		for (const Ref<VoxelInstanceLibraryItem> &item : emitter->get_items()) {
// 			if (item.is_null()) {
// 				continue;
// 			}
// 			if (item == item) {
// 				return true;
// 			}
// 		}
// 	}
// 	return false;
// }

void VoxelInstanceLibrary::unregister_item(Ref<VoxelInstanceLibraryItem> item, const VoxelInstanceEmitter *emitter) {
	// if (any_emitter_has_item(*this, item, emitter)) {
	// 	return;
	// }
	const LayerKey key{ item.ptr(), emitter };
	auto item_to_id_it = _item_to_layer_id.find(key);
	if (item_to_id_it == _item_to_layer_id.end()) {
		ZN_PRINT_ERROR("Item not registered");
		return;
	}
	const int id = item_to_id_it->second;
	_item_to_layer_id.erase(item_to_id_it);
	_layer_id_to_item.erase(id);

	for (IInstanceLibraryListener *listener : _listeners) {
		listener->on_library_item_unregistered(id);
	}

	_packed_data_need_update = true;
}

void VoxelInstanceLibrary::on_emitter_item_added(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) {
	register_item(item, emitter);
}

void VoxelInstanceLibrary::on_emitter_item_removed(VoxelInstanceEmitter *emitter, Ref<VoxelInstanceLibraryItem> item) {
	unregister_item(item, emitter);
}

void VoxelInstanceLibrary::on_emitter_lod_index_changed(
		VoxelInstanceEmitter *emitter,
		const uint8_t previous_lod_index,
		const uint8_t new_lod_index
) {
	ZN_ASSERT_RETURN(previous_lod_index != new_lod_index);

	for (const Ref<VoxelInstanceLibraryItem> &item : emitter->get_items()) {
		if (item.is_null()) {
			continue;
		}
		// TODO Could have a specialized event to just change the LOD and regenerate?
		unregister_item(item, emitter);
		register_item(item, emitter);
	}

	_packed_data_need_update = true;
}

void VoxelInstanceLibrary::on_emitter_generator_changed(VoxelInstanceEmitter *emitter) {
	ZN_ASSERT_RETURN(emitter != nullptr);
	for (IInstanceLibraryListener *listener : _listeners) {
		listener->on_library_emitter_changed(*emitter);
	}
}

void VoxelInstanceLibrary::on_emitter_array_assigned() {
	_packed_data_need_update = true;
}

void VoxelInstanceLibrary::on_library_item_changed(VoxelInstanceLibraryItem *item, ItemChangeType change) {
	ZN_ASSERT_RETURN(item != nullptr);
	IInstanceLibraryListener::ItemChange dst_change;
	switch (change) {
		case ITEM_CHANGE_SCENE:
			dst_change = IInstanceLibraryListener::ITEM_CHANGE_SCENE;
			break;
		case ITEM_CHANGE_VISUAL:
			dst_change = IInstanceLibraryListener::ITEM_CHANGE_VISUAL;
			break;
		default:
			ZN_PRINT_ERROR("Change type not handled");
			return;
	}
	for (auto it = _item_to_layer_id.begin(); it != _item_to_layer_id.end(); ++it) {
		const LayerKey &key = it->first;
		if (key.item == item) {
			const int id = it->second;
			for (IInstanceLibraryListener *listener : _listeners) {
				listener->on_library_item_changed(id, dst_change);
			}
		}
	}
}

const VoxelInstanceLibrary::PackedData &VoxelInstanceLibrary::get_packed_data() const {
	return _packed_data;
}

bool VoxelInstanceLibrary::is_packed_data_needing_update() const {
	return _packed_data_need_update;
}

void VoxelInstanceLibrary::update_packed_data() {
	ZN_PROFILE_SCOPE();

	MutexLock mlock(_packed_data.mutex);

	for (PackedData::Lod &lod : _packed_data.lods) {
		lod.clear();
	}

	for (const Ref<VoxelInstanceEmitter> &emitter : _emitters) {
		if (emitter.is_null()) {
			continue;
		}

		if (emitter->get_generator().is_null()) {
			continue;
		}

		Span<const Ref<VoxelInstanceLibraryItem>> items = emitter->get_items();
		if (items.size() == 0) {
			continue;
		}

		const uint8_t lod_index = emitter->get_lod_index();
		PackedData::Lod &lod = _packed_data.lods[lod_index];

		lod.emitters.push_back(emitter);

		lod.item_counts.push_back(items.size());

		for (const Ref<VoxelInstanceLibraryItem> &item : items) {
			if (item.is_null()) {
				lod.layer_ids.push_back(-1);
			} else {
				const int layer_id = get_layer_id_from_emitter_and_item(item.ptr(), emitter.ptr());
				lod.layer_ids.push_back(layer_id);
			}
		}
	}

	_packed_data_need_update = false;
}

TypedArray<VoxelInstanceEmitter> VoxelInstanceLibrary::_b_get_emitters() const {
	TypedArray<VoxelInstanceEmitter> array;
	zylann::godot::copy_to(array, to_span_const(_emitters));
	return array;
}

void VoxelInstanceLibrary::_b_set_emitters(TypedArray<VoxelInstanceEmitter> array) {
	// TODO Don't diff if there are no listeners
	StdVector<Ref<VoxelInstanceEmitter>> added;
	StdVector<Ref<VoxelInstanceEmitter>> removed;

	diff_set_array(_emitters, array, added, removed);

	for (const Ref<VoxelInstanceEmitter> &emitter : added) {
		emitter->add_listener(this);
		for (const Ref<VoxelInstanceLibraryItem> &item : emitter->get_items()) {
			if (item.is_valid()) {
				register_item(item, emitter.ptr());
			}
		}
	}

	for (const Ref<VoxelInstanceEmitter> &emitter : removed) {
		emitter->remove_listener(this);
		for (const Ref<VoxelInstanceLibraryItem> &item : emitter->get_items()) {
			if (item.is_valid()) {
				unregister_item(item, emitter.ptr());
			}
		}
	}

	for (const Ref<VoxelInstanceEmitter> &emitter : added) {
		for (IInstanceLibraryListener *listener : _listeners) {
			listener->on_library_emitter_changed(**emitter);
		}
	}

	update_packed_data();
}

void VoxelInstanceLibrary::_bind_methods() {
	using Self = VoxelInstanceLibrary;

	ClassDB::bind_method(D_METHOD("set_emitters", "emitters"), &Self::_b_set_emitters);
	ClassDB::bind_method(D_METHOD("get_emitters"), &Self::_b_get_emitters);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY,
					"emitters",
					PROPERTY_HINT_ARRAY_TYPE,
					MAKE_RESOURCE_TYPE_HINT(VoxelInstanceEmitter::get_class_static())
			),
			"set_emitters",
			"get_emitters"
	);
}

} // namespace zylann::voxel
