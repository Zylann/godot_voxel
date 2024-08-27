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

VoxelInstanceLibrary::~VoxelInstanceLibrary() {}

Ref<VoxelInstanceLibraryItem> VoxelInstanceLibrary::get_item_by_layer_id(const int id) const {
	auto it = _layer_id_to_item.find(id);
	ZN_ASSERT_RETURN_V(it != _layer_id_to_item.end(), Ref<VoxelInstanceLibraryItem>());
	return it->second;
}

void VoxelInstanceLibrary::add_listener(IInstanceLibraryListener *listener) {
	ZN_ASSERT_RETURN(listener != nullptr);
	ZN_ASSERT_RETURN(!contains(_listeners, listener));
	_listeners.push_back(listener);
}

void VoxelInstanceLibrary::remove_listener(IInstanceLibraryListener *listener) {
	size_t i;
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

// TODO Identifying items by layer ID is only really useful for multimeshes, scenes could be tagged with it instead.
// This means all scenes could share a same special layer ID per LOD. We may want to refactor that later...

void VoxelInstanceLibrary::register_item(Ref<VoxelInstanceLibraryItem> item, const VoxelInstanceEmitter *emitter) {
	const LayerKey key{ item.ptr(), emitter };
	if (_item_to_layer_id.find(key) != _item_to_layer_id.end()) {
		// Already registered
		return;
	}
	const int id = _layer_id_to_item.rbegin()->first + 1;
	_item_to_layer_id.insert({ key, id });
	_layer_id_to_item.insert({ id, item });

	for (IInstanceLibraryListener *listener : _listeners) {
		listener->on_library_item_registered(id);
	}
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
}

void VoxelInstanceLibrary::on_emitter_generator_changed(VoxelInstanceEmitter *emitter) {
	ZN_ASSERT_RETURN(emitter != nullptr);
	for (IInstanceLibraryListener *listener : _listeners) {
		listener->on_library_emitter_changed(*emitter);
	}
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
		for (const Ref<VoxelInstanceLibraryItem> &item : emitter->get_items()) {
			if (item.is_valid()) {
				register_item(item, emitter.ptr());
			}
		}
	}

	for (const Ref<VoxelInstanceEmitter> &emitter : removed) {
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
					MAKE_RESOURCE_TYPE_HINT(VoxelInstanceLibraryItem::get_class_static())
			),
			"set_emitters",
			"get_emitters"
	);
}

} // namespace zylann::voxel
